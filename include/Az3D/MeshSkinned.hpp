#pragma once

#include "AzVulk/Buffer.hpp"
#include "Az3D/VertexTypes.hpp"

#include <iostream>

namespace Az3D {

struct Bone {
    std::string name;
    int parentIndex = -1;
    glm::mat4 inverseBindMatrix; // from mesh space to bone space
    glm::mat4 localBindTransform; // T/R/S from glTF node

    glm::mat4 localPoseTransform = glm::mat4(1.0f); // modifiable at runtime
};

struct Skeleton {
    std::vector<Bone> bones;
    std::unordered_map<std::string, int> nameToIndex;

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

    void debugPrintHierarchy() const {
        for (size_t i = 0; i < bones.size(); i++) {
            if (bones[i].parentIndex == -1) {
                debugPrintRecursive(static_cast<int>(i), 0);
            }
        }
    }

    private:
    void debugPrintRecursive(int boneIndex, int depth) const {
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


};

struct MeshSkinned {
    std::vector<VertexSkinned> vertices;
    std::vector<uint32_t> indices;

    Skeleton skeleton; // bones + hierarchy

    static SharedPtr<MeshSkinned> loadFromGLTF(const std::string& filePath);

    AzVulk::BufferData vertexBufferData;
    AzVulk::BufferData indexBufferData;
    void createDeviceBuffer(const AzVulk::Device* vkDevice);
};

}