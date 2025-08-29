#include "Az3D/MeshSkinned.hpp"
#include <stdexcept>
#include <glm/gtc/type_ptr.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "Helpers/tiny_gltf.h"

namespace Az3D {

// Compute all global bone transforms
std::vector<glm::mat4> Skeleton::computeGlobalTransforms(const std::vector<glm::mat4>& localPoseTransforms) const {
    std::vector<glm::mat4> globalTransforms(names.size());
    for (size_t i = 0; i < names.size(); i++) {
        int parent = parentIndices[i];
        if (parent == -1) {
            globalTransforms[i] = localPoseTransforms[i];
        } else {
            globalTransforms[i] = globalTransforms[parent] * localPoseTransforms[i];
        }
    }
    return globalTransforms;
}

std::vector<glm::mat4> Skeleton::copyLocalBindToPoseTransforms() const {
    return localBindTransforms;
}

void Skeleton::debugPrintHierarchy() const {
    for (size_t i = 0; i < names.size(); i++) {
        if (parentIndices[i] == -1) {
            debugPrintRecursive(static_cast<int>(i), 0);
        }
    }
}

void Skeleton::debugPrintRecursive(int boneIndex, int depth) const {
    // const Bone& bone = bones[boneIndex];

    // Indentation
    for (int i = 0; i < depth; i++) std::cout << "  ";

    // Print this bone
    std::cout << "- " << names[boneIndex] << " (index " << boneIndex << ")";
    if (parentIndices[boneIndex] != -1) {
        std::cout << " [parent " << parentIndices[boneIndex] << "]";
    }
    std::cout << "\n";

    // Recurse into children
    for (size_t i = 0; i < names.size(); i++) {
        if (parentIndices[i] == boneIndex) {
            debugPrintRecursive(static_cast<int>(i), depth + 1);
        }
    }
}

// Utility: read GLTF accessor as typed array
template<typename T>
void readAccessor(const tinygltf::Model& model, const tinygltf::Accessor& accessor, std::vector<T>& out) {
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

SharedPtr<MeshSkinned> MeshSkinned::loadFromGLTF(const std::string& filePath) {
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

        meshSkinned->skeleton.names.reserve(skin.joints.size());
        meshSkinned->skeleton.parentIndices.reserve(skin.joints.size());
        meshSkinned->skeleton.inverseBindMatrices.reserve(skin.joints.size());
        meshSkinned->skeleton.localBindTransforms.reserve(skin.joints.size());

        for (size_t i = 0; i < skin.joints.size(); i++) {
            int nodeIndex = skin.joints[i];
            const auto& node = model.nodes[nodeIndex];

            
            std::string boneName = node.name.empty() ? ("bone_" + std::to_string(i)) : node.name;

            // Fill parent index: if node has a parent that's also a joint, link it
            int boneParentIndex = -1;
            for (size_t p = 0; p < model.nodes.size(); p++) {
                const auto& parent = model.nodes[p];
                for (int childIdx : parent.children) {
                    if (childIdx == nodeIndex) {
                        // Check if parent is part of this skin
                        auto it = std::find(skin.joints.begin(), skin.joints.end(), p);
                        if (it != skin.joints.end()) {
                            boneParentIndex = static_cast<int>(std::distance(skin.joints.begin(), it));
                        }
                    }
                }
            }

            glm::mat4 boneInverseBindMatrix = (ibms.size() > i) ? ibms[i] : glm::mat4(1.0f);

            glm::mat4 boneLocalBindTransform;
            if (node.matrix.size() == 16) {
                boneLocalBindTransform = glm::make_mat4(node.matrix.data());
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
                boneLocalBindTransform = t * r * s;
            }

            glm::mat4 boneLocalPoseTransform = boneLocalBindTransform;

            meshSkinned->skeleton.nameToIndex[boneName] = static_cast<int>(meshSkinned->skeleton.names.size());

            meshSkinned->skeleton.names.push_back(boneName);
            meshSkinned->skeleton.parentIndices.push_back(boneParentIndex);
            meshSkinned->skeleton.inverseBindMatrices.push_back(boneInverseBindMatrix);
            meshSkinned->skeleton.localBindTransforms.push_back(boneLocalBindTransform);
        }
    }

    // TODO: load animations from model.animations if needed

    // meshSkinned->skeleton.debugPrintHierarchy();

    return meshSkinned;
}


MeshSkinnedGroup::MeshSkinnedGroup(const AzVulk::Device* vkDevice) :
    vkDevice(vkDevice) {}

size_t MeshSkinnedGroup::addMeshSkinned(SharedPtr<MeshSkinned> mesh) {
    meshes.push_back(mesh);
    return meshes.size() - 1;
}



} // namespace Az3D