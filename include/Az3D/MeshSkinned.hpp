#pragma once

#include "AzVulk/Buffer.hpp"
#include "Az3D/VertexTypes.hpp"

#include <iostream>

namespace Az3D {

struct Skeleton {

    // Bone SoA
    std::vector<std::string> names;
    std::vector<int> parentIndices;
    std::vector<glm::mat4> inverseBindMatrices;
    std::vector<glm::mat4> localBindTransforms;

    std::unordered_map<std::string, int> nameToIndex;

    std::vector<glm::mat4> computeGlobalTransforms(const std::vector<glm::mat4>& localPoseTransforms) const;
    std::vector<glm::mat4> copyLocalBindToPoseTransforms() const;

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

    // Index-based mesh storage
    SharedPtrVec<MeshSkinned> meshes;

    const AzVulk::Device* vkDevice;
    void createDeviceBuffers();
};

}