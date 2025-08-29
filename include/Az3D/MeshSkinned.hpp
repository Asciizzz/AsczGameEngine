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

    glm::mat4 Skeleton::computeGlobalTransform(int boneIndex) const;
    std::vector<glm::mat4> Skeleton::computeGlobalTransforms() const;

    void debugPrintHierarchy() const;
    void debugPrintRecursive(int boneIndex, int depth) const;
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


class MeshSkinnedGroup {
public:
    MeshSkinnedGroup(const AzVulk::Device* vkDevice);
    MeshSkinnedGroup(const MeshSkinnedGroup&) = delete;
    MeshSkinnedGroup& operator=(const MeshSkinnedGroup&) = delete;

    size_t addMeshSkinned(SharedPtr<MeshSkinned> mesh);
    size_t loadFromOBJ(std::string filePath);

    // Index-based mesh storage
    SharedPtrVec<MeshSkinned> meshes;

    const AzVulk::Device* vkDevice;
    void createDeviceBuffers();
};

}