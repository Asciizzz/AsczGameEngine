#include "Az3D/MeshSkinned.hpp"
#include <stdexcept>
#include <glm/gtc/type_ptr.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

namespace Az3D {


// Compute global transform of a bone
glm::mat4 Skeleton::computeGlobalTransform(int boneIndex) const {
    const Bone& bone = bones[boneIndex];
    if (bone.parentIndex == -1) {
        return bone.localPoseTransform;
    }
    return computeGlobalTransform(bone.parentIndex) * bone.localPoseTransform;
}

// Compute all global bone transforms
std::vector<glm::mat4> Skeleton::computeGlobalTransforms() const {
    std::vector<glm::mat4> result(bones.size());
    for (size_t i = 0; i < bones.size(); i++) {
        result[i] = computeGlobalTransform(static_cast<int>(i));
    }
    return result;
}

void Skeleton::debugPrintHierarchy() const {
    for (size_t i = 0; i < bones.size(); i++) {
        if (bones[i].parentIndex == -1) {
            debugPrintRecursive(static_cast<int>(i), 0);
        }
    }
}

void Skeleton::debugPrintRecursive(int boneIndex, int depth) const {
    const Bone& bone = bones[boneIndex];

    // Indentation
    for (int i = 0; i < depth; i++) std::cout << "  ";

    // Print this bone
    std::cout << "- " << bone.name << " (index " << boneIndex << ")";
    if (bone.parentIndex != -1) {
        std::cout << " [parent " << bone.parentIndex << "]";
    }
    std::cout << "\n";

    // Recurse into children
    for (size_t i = 0; i < bones.size(); i++) {
        if (bones[i].parentIndex == boneIndex) {
            debugPrintRecursive(static_cast<int>(i), depth + 1);
        }
    }
}

// Utility: read GLTF accessor as typed array
template<typename T>
void readAccessor(const tinygltf::Model& model, const tinygltf::Accessor& accessor, std::vector<T>& out)
{
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buf = model.buffers[view.buffer];

    const unsigned char* dataPtr = buf.data.data() + view.byteOffset + accessor.byteOffset;
    size_t stride = accessor.ByteStride(view);
    out.resize(accessor.count);

    for (size_t i = 0; i < accessor.count; i++) {
        const void* ptr = dataPtr + stride * i;
        memcpy(&out[i], ptr, sizeof(T));
    }
}

SharedPtr<MeshSkinned> MeshSkinned::loadFromGLTF(const std::string& filePath)
{
    auto meshSkinned = std::make_shared<MeshSkinned>();

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;

    bool ok = loader.LoadASCIIFromFile(&model, &err, &warn, filePath);
    if (!ok) {
        throw std::runtime_error("GLTF load error: " + err);
    }

    if (model.meshes.empty()) {
        throw std::runtime_error("GLTF has no meshes: " + filePath);
    }

    const tinygltf::Mesh& mesh = model.meshes[0];
    size_t vertexOffset = 0;

    // Merge all primitives into one vertex/index buffer
    for (const auto& primitive : mesh.primitives) {
        size_t vertexCount = model.accessors[primitive.attributes.at("POSITION")].count;
        std::vector<glm::vec3> positions, normals;
        std::vector<glm::vec4> tangents, weights;
        std::vector<glm::vec2> uvs;
        std::vector<glm::uvec4> joints;

        // Read attributes (skip missing gracefully)
        if (primitive.attributes.count("POSITION"))
            readAccessor(model, model.accessors[primitive.attributes.at("POSITION")], positions);
        if (primitive.attributes.count("NORMAL"))
            readAccessor(model, model.accessors[primitive.attributes.at("NORMAL")], normals);
        if (primitive.attributes.count("TANGENT"))
            readAccessor(model, model.accessors[primitive.attributes.at("TANGENT")], tangents);
        if (primitive.attributes.count("TEXCOORD_0"))
            readAccessor(model, model.accessors[primitive.attributes.at("TEXCOORD_0")], uvs);
        if (primitive.attributes.count("JOINTS_0"))
            readAccessor(model, model.accessors[primitive.attributes.at("JOINTS_0")], joints);
        if (primitive.attributes.count("WEIGHTS_0"))
            readAccessor(model, model.accessors[primitive.attributes.at("WEIGHTS_0")], weights);

        // Expand into vertices
        for (size_t i = 0; i < vertexCount; i++) {
            VertexSkinned v{};
            v.pos_tu     = glm::vec4(positions.size() > i ? positions[i] : glm::vec3(0.0f),
                                     uvs.size() > i ? uvs[i].x : 0.0f);
            v.nrml_tv    = glm::vec4(normals.size() > i ? normals[i] : glm::vec3(0.0f),
                                     uvs.size() > i ? uvs[i].y : 0.0f);
            v.tangent    = tangents.size() > i ? tangents[i] : glm::vec4(1,0,0,1);
            v.boneIDs    = joints.size() > i ? joints[i] : glm::uvec4(0);
            v.weights    = weights.size() > i ? weights[i] : glm::vec4(0,0,0,0);

            meshSkinned->vertices.push_back(v);
        }

        // Indices
        if (primitive.indices >= 0) {
            const auto& indexAccessor = model.accessors[primitive.indices];
            const auto& indexBufferView = model.bufferViews[indexAccessor.bufferView];
            const auto& indexBuffer = model.buffers[indexBufferView.buffer];
            const unsigned char* dataPtr = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;
            size_t stride = indexAccessor.ByteStride(indexBufferView);

            for (size_t i = 0; i < indexAccessor.count; i++) {
                uint32_t index = 0;
                switch (indexAccessor.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        index = *((uint8_t*)(dataPtr + stride * i));
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        index = *((uint16_t*)(dataPtr + stride * i));
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        index = *((uint32_t*)(dataPtr + stride * i));
                        break;
                    default:
                        throw std::runtime_error("Unsupported index component type");
                }
                meshSkinned->indices.push_back(index + static_cast<uint32_t>(vertexOffset));
            }
        }

        vertexOffset += vertexCount;
    }

    // Skeleton (take first skin if present)
    if (!model.skins.empty()) {
        const tinygltf::Skin& skin = model.skins[0];

        // Inverse bind matrices
        std::vector<glm::mat4> ibms;
        if (skin.inverseBindMatrices >= 0) {
            readAccessor(model, model.accessors[skin.inverseBindMatrices], ibms);
        }

        meshSkinned->skeleton.bones.reserve(skin.joints.size());

        for (size_t i = 0; i < skin.joints.size(); i++) {
            int nodeIndex = skin.joints[i];
            const auto& node = model.nodes[nodeIndex];

            Bone bone;
            bone.name = node.name.empty() ? ("bone_" + std::to_string(i)) : node.name;

            // Fill parent index: if node has a parent that's also a joint, link it
            bone.parentIndex = -1;
            for (size_t p = 0; p < model.nodes.size(); p++) {
                const auto& parent = model.nodes[p];
                for (int childIdx : parent.children) {
                    if (childIdx == nodeIndex) {
                        // Check if parent is part of this skin
                        auto it = std::find(skin.joints.begin(), skin.joints.end(), p);
                        if (it != skin.joints.end()) {
                            bone.parentIndex = static_cast<int>(std::distance(skin.joints.begin(), it));
                        }
                    }
                }
            }

            bone.inverseBindMatrix = (ibms.size() > i) ? ibms[i] : glm::mat4(1.0f);

            if (node.matrix.size() == 16) {
                bone.localBindTransform = glm::make_mat4(node.matrix.data());
            } else {
                glm::mat4 t = glm::translate(glm::mat4(1.0f),
                                            node.translation.size() ? glm::vec3(node.translation[0], node.translation[1], node.translation[2]) : glm::vec3(0));
                glm::mat4 r = glm::mat4(1.0f);
                if (node.rotation.size() == 4) {
                    glm::quat q(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
                    r = glm::mat4_cast(q);
                }
                glm::mat4 s = glm::scale(glm::mat4(1.0f),
                                        node.scale.size() ? glm::vec3(node.scale[0], node.scale[1], node.scale[2]) : glm::vec3(1));
                bone.localBindTransform = t * r * s;
            }

            bone.localPoseTransform = bone.localBindTransform;

            meshSkinned->skeleton.nameToIndex[bone.name] = static_cast<int>(meshSkinned->skeleton.bones.size());
            meshSkinned->skeleton.bones.push_back(bone);
        }
    }


    // TODO: load animations from model.animations if needed

    return meshSkinned;
}

}