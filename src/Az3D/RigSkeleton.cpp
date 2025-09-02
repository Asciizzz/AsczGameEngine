#include "Az3D/RigSkeleton.hpp"
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace Az3D;


void RigSkeleton::debugPrintHierarchy() const {
    for (size_t i = 0; i < names.size(); ++i) {
        if (parentIndices[i] == -1) {
            debugPrintRecursive(static_cast<int>(i), 0);
        }
    }
}

void RigSkeleton::debugPrintRecursive(int boneIndex, int depth) const {
    // Indentation
    for (int i = 0; i < depth; ++i) std::cout << "| ";

    // Print this bone
    std::cout << "| " << names[boneIndex] << " (index " << boneIndex << ")";
    if (parentIndices[boneIndex] != -1) {
        std::cout << " [parent " << parentIndices[boneIndex] << "]";
    }
    std::cout << "\n";

    // Recurse into children
    for (size_t i = 0; i < names.size(); ++i) {
        if (parentIndices[i] == boneIndex) {
            debugPrintRecursive(static_cast<int>(i), depth + 1);
        }
    }
}


void RigDemo::computeAllTransforms() {
    // We need to compute transforms in dependency order (parents before children)
    // First, identify root bones and process them recursively
    
    std::vector<bool> processed(rigSkeleton->names.size(), false);
    
    // Process all bones recursively, starting from roots
    for (size_t i = 0; i < rigSkeleton->names.size(); ++i) {
        if (rigSkeleton->parentIndices[i] == -1 && !processed[i]) {
            computeBoneRecursive(i, processed);
        }
    }
    
    // Handle any orphaned bones (shouldn't happen with proper hierarchy)
    for (size_t i = 0; i < rigSkeleton->names.size(); ++i) {
        if (!processed[i]) {
            std::cout << "Warning: Orphaned bone " << i << " (" << rigSkeleton->names[i] << ")\n";
            globalPoseTransforms[i] = localPoseTransforms[i];
            finalTransforms[i] = globalPoseTransforms[i] * rigSkeleton->inverseBindMatrices[i];
            processed[i] = true;
        }
    }
}

void RigDemo::computeBoneRecursive(size_t boneIndex, std::vector<bool>& processed) {
    if (processed[boneIndex]) return;
    
    int parent = rigSkeleton->parentIndices[boneIndex];
    
    if (parent == -1) {
        // Root bone
        globalPoseTransforms[boneIndex] = localPoseTransforms[boneIndex];
    } else {
        // Ensure parent is computed first
        computeBoneRecursive(parent, processed);
        globalPoseTransforms[boneIndex] = globalPoseTransforms[parent] * localPoseTransforms[boneIndex];
    }
    
    finalTransforms[boneIndex] = globalPoseTransforms[boneIndex] * rigSkeleton->inverseBindMatrices[boneIndex];
    processed[boneIndex] = true;
    
    // Process all children
    for (size_t i = 0; i < rigSkeleton->names.size(); ++i) {
        if (rigSkeleton->parentIndices[i] == static_cast<int>(boneIndex) && !processed[i]) {
            computeBoneRecursive(i, processed);
        }
    }
}

void RigDemo::init(const AzVulk::Device* vkDevice, const SharedPtr<RigSkeleton>& skeleton) {
    using namespace AzVulk;

    rigSkeleton = skeleton;

    skeleton->debugPrintHierarchy();

    localPoseTransforms.resize(skeleton->names.size());
    globalPoseTransforms.resize(skeleton->names.size());
    finalTransforms.resize(skeleton->names.size());
    
    localPoseTransforms = skeleton->localBindTransforms;

    computeAllTransforms();

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
    if (index >= rigSkeleton->names.size()) {
        return glm::mat4(1.0f);
    }
    return rigSkeleton->localBindTransforms[index];
}

void RigDemo::funFunction(float dTime) {
    // Messed up some bone idk
    funAccumTimeValue += dTime;

    // I know that it may looks like magic numbers right now
    // But i can assure you, they... are indeed magic numbers lol
    // But don't worry, this is just a playground to test out
    // the new rigging system, once everything is implemented
    // There will be actual robust bone handler

    float partRotMax = 0.12f;
    float partRot = partRotMax * sin(funAccumTimeValue);

    // Extract transformation data from the mat4
    glm::mat4 pose102 = getBindPose(102);
    glm::mat4 pose104 = getBindPose(104);

    // Method 1: Using GLM decompose (if available)
    glm::vec3 translation102, scale102, skew;
    glm::vec4 perspective;
    glm::quat rotation102;
    bool decompose_success102 = glm::decompose(pose102, scale102, rotation102, translation102, skew, perspective);

    glm::vec3 translation104, scale104;
    glm::quat rotation104;
    bool decompose_success104 = glm::decompose(pose104, scale104, rotation104, translation104, skew, perspective);

    if (decompose_success102 && decompose_success104) {
        // Apply your modifications
        translation102.x += partRot;  // Add translation offset
        translation104.x += partRot;  // Add translation offset

        // Reconstruct the transformation matrices
        glm::mat4 translationMat102 = glm::translate(glm::mat4(1.0f), translation102);
        glm::mat4 rotationMat102 = glm::mat4_cast(rotation102);
        glm::mat4 scaleMat102 = glm::scale(glm::mat4(1.0f), scale102);
        
        glm::mat4 translationMat104 = glm::translate(glm::mat4(1.0f), translation104);
        glm::mat4 rotationMat104 = glm::mat4_cast(rotation104);
        glm::mat4 scaleMat104 = glm::scale(glm::mat4(1.0f), scale104);

        // Reconstruct final transformation matrices (TRS order)
        localPoseTransforms[102] = translationMat102 * rotationMat102 * scaleMat102;
        localPoseTransforms[104] = translationMat104 * rotationMat104 * scaleMat104;
    } else {
        // Fallback: Manual extraction (if GLM decompose fails)
        // Extract translation (4th column)
        glm::vec3 manual_translation102 = glm::vec3(pose102[3]);
        glm::vec3 manual_translation104 = glm::vec3(pose104[3]);
        
        // Extract scale (length of first 3 columns)
        glm::vec3 manual_scale102 = glm::vec3(
            glm::length(glm::vec3(pose102[0])),
            glm::length(glm::vec3(pose102[1])),
            glm::length(glm::vec3(pose102[2]))
        );
        glm::vec3 manual_scale104 = glm::vec3(
            glm::length(glm::vec3(pose104[0])),
            glm::length(glm::vec3(pose104[1])),
            glm::length(glm::vec3(pose104[2]))
        );
        
        // Extract rotation matrix (normalize first 3 columns)
        glm::mat3 manual_rotation102 = glm::mat3(
            glm::vec3(pose102[0]) / manual_scale102.x,
            glm::vec3(pose102[1]) / manual_scale102.y,
            glm::vec3(pose102[2]) / manual_scale102.z
        );
        glm::mat3 manual_rotation104 = glm::mat3(
            glm::vec3(pose104[0]) / manual_scale104.x,
            glm::vec3(pose104[1]) / manual_scale104.y,
            glm::vec3(pose104[2]) / manual_scale104.z
        );
        
        // Apply modifications
        manual_translation102.x += partRot;
        manual_translation104.x += partRot;
        
        // Reconstruct matrices
        localPoseTransforms[102] = glm::translate(glm::mat4(1.0f), manual_translation102) * 
                                  glm::mat4(manual_rotation102) * 
                                  glm::scale(glm::mat4(1.0f), manual_scale102);
        localPoseTransforms[104] = glm::translate(glm::mat4(1.0f), manual_translation104) * 
                                  glm::mat4(manual_rotation104) * 
                                  glm::scale(glm::mat4(1.0f), manual_scale104);
    }

    computeAllTransforms();
}