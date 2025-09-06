#include "Az3D/RigDemo.hpp"
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace Az3D;

// Extract TRS components from a matrix
FunTransform extractTransform(const glm::mat4& matrix) {
    FunTransform transform;
    
    // Try GLM decompose first
    glm::vec3 skew;
    glm::vec4 perspective;
    bool decompose_success = glm::decompose(matrix, transform.scale, transform.rotation, 
                                          transform.translation, skew, perspective);
    
    if (!decompose_success) {
        // Fallback: Manual extraction
        transform.translation = glm::vec3(matrix[3]);
        
        // Extract scale (length of first 3 columns)
        transform.scale = glm::vec3(
            glm::length(glm::vec3(matrix[0])),
            glm::length(glm::vec3(matrix[1])),
            glm::length(glm::vec3(matrix[2]))
        );
        
        // Extract rotation matrix (normalize first 3 columns)
        glm::mat3 rotationMatrix = glm::mat3(
            glm::vec3(matrix[0]) / transform.scale.x,
            glm::vec3(matrix[1]) / transform.scale.y,
            glm::vec3(matrix[2]) / transform.scale.z
        );
        
        // Convert rotation matrix to quaternion
        transform.rotation = glm::quat_cast(rotationMatrix);
    }
    
    return transform;
}

// Create matrix from TRS components
glm::mat4 constructMatrix(const FunTransform& transform) {
    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), transform.translation);
    glm::mat4 rotationMat = glm::mat4_cast(transform.rotation);
    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), transform.scale);
    
    // TRS order: Translation * Rotation * Scale
    return translationMat * rotationMat * scaleMat;
}


void RigDemo::computeAllTransforms() {
    // We need to compute transforms in dependency order (parents before children)
    // First, identify root bones and process them recursively
    
    std::vector<bool> processed(skeleton.names.size(), false);
    
    // Process all bones recursively, starting from roots
    for (size_t i = 0; i < skeleton.names.size(); ++i) {
        if (skeleton.parentIndices[i] == -1 && !processed[i]) {
            computeBoneRecursive(i, processed);
        }
    }
    
    // Handle any orphaned bones (shouldn't happen with proper hierarchy)
    for (size_t i = 0; i < skeleton.names.size(); ++i) {
        if (!processed[i]) {
            std::cout << "Warning: Orphaned bone " << i << " (" << skeleton.names[i] << ")\n";
            globalPoseTransforms[i] = localPoseTransforms[i];
            finalTransforms[i] = globalPoseTransforms[i] * skeleton.inverseBindMatrices[i];
            processed[i] = true;
        }
    }
}

void RigDemo::computeBoneRecursive(size_t boneIndex, std::vector<bool>& processed) {
    if (processed[boneIndex]) return;
    
    int parent = skeleton.parentIndices[boneIndex];
    
    if (parent == -1) {
        // Root bone
        globalPoseTransforms[boneIndex] = localPoseTransforms[boneIndex];
    } else {
        // Ensure parent is computed first
        computeBoneRecursive(parent, processed);
        globalPoseTransforms[boneIndex] = globalPoseTransforms[parent] * localPoseTransforms[boneIndex];
    }
    
    finalTransforms[boneIndex] = globalPoseTransforms[boneIndex] * skeleton.inverseBindMatrices[boneIndex];
    processed[boneIndex] = true;
    
    // Process all children
    for (size_t i = 0; i < skeleton.names.size(); ++i) {
        if (skeleton.parentIndices[i] == static_cast<int>(boneIndex) && !processed[i]) {
            computeBoneRecursive(i, processed);
        }
    }
}

void RigDemo::init(const AzVulk::Device* vkDevice, const TinySkeleton& skeleton, size_t modelIndex) {
    using namespace AzVulk;

    this->skeleton = skeleton;
    this->modelIndex = modelIndex;

    // skeleton.debugPrintHierarchy();

    localPoseTransforms.resize(skeleton.names.size());
    globalPoseTransforms.resize(skeleton.names.size());
    finalTransforms.resize(skeleton.names.size());
    
    localPoseTransforms = skeleton.localBindTransforms;

    computeAllTransforms();

    size_t n = localPoseTransforms.size();
    states.resize(n);
    for (size_t i = 0; i < n; ++i) {
        FunTransform bt = extractTransform(localPoseTransforms[i]);
        states[i].localTransform = bt;
        states[i].angVel = glm::vec3(0.0f);

        // Tweak default param per bone type heuristically
        states[i].stiffness = 50.0f;
        states[i].damping   = 8.0f;
        states[i].inertia   = 1.0f;
        states[i].maxAngVel = 30.0f;
    }



    finalPoseBuffer.initVkDevice(vkDevice);

    finalPoseBuffer.setProperties(
        finalTransforms.size() * sizeof(glm::mat4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    finalPoseBuffer.createBuffer();
    finalPoseBuffer.mapMemory();
    updateBuffer();

    descLayout.init(vkDevice->lDevice);
    descLayout.create({
        DescLayout::BindInfo{0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}
    });

    descPool.init(vkDevice->lDevice);
    descPool.create({ {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} }, 1);

    descSet.init(vkDevice->lDevice);
    descSet.allocate(descPool.get(), descLayout.get(), 1);

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = finalPoseBuffer.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(glm::mat4) * finalTransforms.size();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descSet.get();
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(vkDevice->lDevice, 1, &write, 0, nullptr);
}

void RigDemo::updateBuffer() {
    finalPoseBuffer.copyData(finalTransforms.data());
}



glm::mat4 RigDemo::getBindPose(size_t index) {
    if (index >= skeleton.names.size()) {
        return glm::mat4(1.0f);
    }
    return skeleton.localBindTransforms[index];
}





// Convert a small angular velocity vector (omega) into a quaternion delta for dt.
// Uses the approximation q ~= exp(0.5 * dt * [0, omega]), but implemented robustly.
inline glm::quat smallAngVelToQuat(const glm::vec3& omega, float dt) {
    // We want quaternion dq such that dq * q applies rotation of angle = |omega|*dt around axis = normalized(omega)
    glm::vec3 v = omega * (0.5f * dt); // factor 0.5 comes from quaternion exp properties
    float angle = glm::length(v);
    if (angle < 1e-8f) {
        // linear approx: q ~ [1, v]
        return glm::quat(1.0f, v.x, v.y, v.z);
    } else {
        glm::vec3 axis = v / angle;
        // note: the quaternion representing rotation by theta is angleAxis(theta, axis)
        // our quaternion should represent rotation by 2*angle because of the earlier factor
        return glm::angleAxis(2.0f * angle, axis);
    }
}

inline void quatToAxisAngleSigned(const glm::quat& q, glm::vec3& outAxis, float& outAngle) {
    // ensure normalized
    glm::quat qq = glm::normalize(q);
    outAngle = 2.0f * std::acos(glm::clamp(qq.w, -1.0f, 1.0f)); // [0, 2pi]
    float s = std::sqrt(1.0f - qq.w * qq.w);
    if (s < 1e-6f) {
        // angle is ~0 -> arbitrary axis
        outAxis = glm::vec3(1,0,0);
    } else {
        outAxis = glm::vec3(qq.x, qq.y, qq.z) / s;
    }
    // Make angle signed in [-pi, pi] for shortest path
    if (outAngle > glm::pi<float>()) outAngle -= 2.0f * glm::pi<float>();
}



void RigDemo::funFunction(const FunParams& params) {
    // Messed up some bone idk
    float dTime = params.customFloat[0];
    funAccumTimeValue += dTime;

    size_t boneCount = skeleton.names.size();

    return;
    /*
    std::vector<FunTransform> localTransforms(boneCount);
    for (size_t i = 0; i < boneCount; ++i) {
        localTransforms[i] = extractTransform(localPoseTransforms[i]);
    }

    for (size_t i = 0; i < boneCount; ++i) {
        BonePhysicsState& st = states[i];
        FunTransform& target = localTransforms[i]; // target local orientation

        // Current and target quaternions (local)
        glm::quat qCurr = st.localTransform.rotation;
        glm::quat qTarget = target.rotation;

        // qErr = qTarget * inverse(qCurr)  => rotation to apply to current to reach target
        glm::quat qErr = qTarget * glm::inverse(qCurr);

        // axis-angle signed
        glm::vec3 axis;
        float angle;
        quatToAxisAngleSigned(qErr, axis, angle);

        // Spring torque approximated as vector: axis * (angle * stiffness)
        glm::vec3 springTorque = axis * (angle * st.stiffness);

        // Damping torque
        glm::vec3 dampingTorque = - st.damping * st.angVel;

        // Net torque -> angular acceleration
        glm::vec3 netTorque = springTorque + dampingTorque;
        glm::vec3 angAccel = netTorque / st.inertia;

        // Semi-implicit Euler integration for angular velocity
        st.angVel += angAccel * dTime;

        // clamp angular velocity magnitude
        float mag = glm::length(st.angVel);
        if (mag > st.maxAngVel) {
            st.angVel *= (st.maxAngVel / mag);
        }

        // Integrate rotation using small-angle quaternion
        glm::quat dq = smallAngVelToQuat(st.angVel, dTime);
        st.localTransform.rotation = glm::normalize(dq * st.localTransform.rotation);

        // Optional angular limit relative to bind
        if (st.useLimit) {
            glm::quat qRel = glm::inverse(qTarget) * st.localTransform.rotation; // rotation from bind->current
            glm::vec3 aaxis;
            float aangle;
            quatToAxisAngleSigned(qRel, aaxis, aangle);
            float limitRad = glm::radians(st.limitAngleDegrees);
            if (std::abs(aangle) > limitRad && limitRad > 1e-6f) {
                // bring it back toward bind by slerp ratio
                float ratio = limitRad / std::abs(aangle);
                st.localTransform.rotation = glm::slerp(qTarget, st.localTransform.rotation, ratio);
                st.angVel *= 0.5f; // damp when hitting limit
            }
        }

        localPoseTransforms[i] = constructMatrix(st.localTransform);
    } // end bones loop

    */
    
    float sinAccum = sin(funAccumTimeValue);

    // // Rotate root (for weird ass model that is rotated 90 degree)
    // FunTransform transform0 = extractTransform(getBindPose(0));
    // transform0.rotation = glm::rotate(transform0.rotation, glm::radians(90.0f), glm::vec3(1,0,0));
    // localPoseTransforms[0] = constructMatrix(transform0);

    // Rotate spine
    size_t spineIndex = 86;
    float partRotSpine = 20.0f * sinAccum;
    FunTransform transform1 = extractTransform(getBindPose(spineIndex));
    transform1.rotation = glm::rotate(transform1.rotation, glm::radians(partRotSpine), glm::vec3(0,1,0));

    localPoseTransforms[spineIndex] = constructMatrix(transform1);

    // Rotate shoulder (right 168 left 130)
    size_t shoulderLeftIndex = 280;
    size_t shoulderRightIndex = 327;
    float partRotShoulderLeft = 28.0f * sinAccum - 10.0f;
    float partRotShoulderRight = 18.0f * sin(1.1f * funAccumTimeValue) - 10.0f;
    FunTransform transformShoulderLeft = extractTransform(getBindPose(shoulderLeftIndex));
    FunTransform transformShoulderRight = extractTransform(getBindPose(shoulderRightIndex));
    transformShoulderLeft.rotation = glm::rotate(transformShoulderLeft.rotation, glm::radians(partRotShoulderLeft), glm::vec3(1,0,0));
    transformShoulderRight.rotation = glm::rotate(transformShoulderRight.rotation, glm::radians(partRotShoulderRight), glm::vec3(1,0,0));

    localPoseTransforms[shoulderLeftIndex] = constructMatrix(transformShoulderLeft);
    localPoseTransforms[shoulderRightIndex] = constructMatrix(transformShoulderRight);


    
    // Rotate tail 
    size_t tailRootIndex = 403;
    size_t tailEndIndex = 409;
    
    float partRotTail = 30.0f * sinAccum;
    FunTransform transformTailRoot = extractTransform(getBindPose(tailRootIndex));
    transformTailRoot.rotation = glm::rotate(transformTailRoot.rotation, glm::radians(partRotTail), glm::vec3(1,0,0));

    localPoseTransforms[tailRootIndex] = constructMatrix(transformTailRoot);

    for (int i = tailRootIndex + 1; i <= tailEndIndex; ++i) {
        FunTransform transform = extractTransform(getBindPose(i));
        float partRot = -5.0f * sin(funAccumTimeValue + i) + 10.0f;
        transform.rotation = glm::rotate(transform.rotation, glm::radians(partRot), glm::vec3(1,0,0));

        localPoseTransforms[i] = constructMatrix(transform);
    }

    /*
    // Rotate (index 2)
    float partRotSpine = 10.0f * sinAccum;
    FunTransform transform1 = extractTransform(getBindPose(2));
    transform1.rotation = glm::rotate(transform1.rotation, glm::radians(partRotSpine), glm::vec3(1,0,0));

    localPoseTransforms[2] = constructMatrix(transform1);

    // Rotate back pack (index 76) opposite from spine
    float partRotBackPack = -5.0f * sinAccum;
    FunTransform transform76 = extractTransform(getBindPose(76));
    transform76.rotation = glm::rotate(transform76.rotation, glm::radians(partRotBackPack), glm::vec3(1,0,0));

    localPoseTransforms[76] = constructMatrix(transform76);

    // Rotate chest (index 3)
    float partRotChest = 15.0f * sinAccum;
    FunTransform transform3 = extractTransform(getBindPose(3));

    glm::vec3 t3normal = glm::normalize(glm::vec3(0.2, 1.0, 0.0));
    transform3.rotation = glm::rotate(transform3.rotation, glm::radians(partRotChest), t3normal);

    localPoseTransforms[3] = constructMatrix(transform3);

    // Rotate shoulder (right 24 left 50)
    float partRotShoulder1 = 18.0f * sinAccum - 10.0f;
    float partRotShoulder2 = 18.0f * sin(1.1f * funAccumTimeValue) - 10.0f;
    FunTransform transform24 = extractTransform(getBindPose(24));
    FunTransform transform50 = extractTransform(getBindPose(50));
    transform24.rotation = glm::rotate(transform24.rotation, glm::radians(partRotShoulder1), glm::vec3(1,0,0));
    transform50.rotation = glm::rotate(transform50.rotation, glm::radians(partRotShoulder2), glm::vec3(1,0,0));

    // transform24.translation = glm::vec3(-0.15f * sinAccum, 0.0f, 0.0f);
    // transform50.translation = glm::vec3(+0.15f * sinAccum, 0.0f, 0.0f);

    localPoseTransforms[24] = constructMatrix(transform24);
    localPoseTransforms[50] = constructMatrix(transform50);
    //*/

    /*
    // Move eyes from left to right (index 102 - left and 104 - right)
    float partMoveEye = 0.12f * sinAccum;
    FunTransform transform102 = extractTransform(getBindPose(102));
    FunTransform transform104 = extractTransform(getBindPose(104));
    transform102.translation.y += partMoveEye;
    transform104.translation.y += partMoveEye;

    localPoseTransforms[102] = constructMatrix(transform102);
    localPoseTransforms[104] = constructMatrix(transform104);

    // Rotate spine (index 1)
    float partRotSpine = 30.0f * sinAccum;
    FunTransform transform1 = extractTransform(getBindPose(1));
    transform1.rotation = glm::rotate(transform1.rotation, glm::radians(partRotSpine), glm::vec3(1,0,0));

    localPoseTransforms[1] = constructMatrix(transform1);

    // Rotate shoulders (index 3 - left, index 27 - right)
    float partRotShoulder1 = 25.0f * sinAccum - 15.0f;
    float partRotShoulder2 = 25.0f * sin(funAccumTimeValue * 1.1) - 15.0f;
    FunTransform transform3 = extractTransform(getBindPose(3));
    FunTransform transform27 = extractTransform(getBindPose(27));
    transform3.rotation = glm::rotate(transform3.rotation, glm::radians(partRotShoulder1), glm::vec3(1,0,0));
    transform27.rotation = glm::rotate(transform27.rotation, glm::radians(partRotShoulder2), glm::vec3(1,0,0));

    localPoseTransforms[3] = constructMatrix(transform3);
    localPoseTransforms[27] = constructMatrix(transform27);

    // Neck (index 51) rotate same way as spine but WAY more subtle
    float partRotNeck = partRotSpine * 0.4f * sin(funAccumTimeValue * 1.5f);
    FunTransform transform51 = extractTransform(getBindPose(51));
    transform51.rotation = glm::rotate(transform51.rotation, glm::radians(partRotNeck), glm::vec3(1,0,0));

    localPoseTransforms[51] = constructMatrix(transform51);

    // Tail (index 110) rotate same way as spine
    float partRotTail = 30.0f * sinAccum;
    FunTransform transform110 = extractTransform(getBindPose(110));
    transform110.rotation = glm::rotate(transform110.rotation, glm::radians(partRotTail), glm::vec3(1,0,0));

    localPoseTransforms[110] = constructMatrix(transform110);

    // Index 111 -> 117 has custom rotate script
    for (int i = 111; i <= 117; ++i) {
        FunTransform transform = extractTransform(getBindPose(i));
        float partRot = -20.0f * sin(funAccumTimeValue + i);
        transform.rotation = glm::rotate(transform.rotation, glm::radians(partRot), glm::vec3(1,0,0));

        localPoseTransforms[i] = constructMatrix(transform);
    }
    //*/

    computeAllTransforms();

}