#pragma once

#include "AzVulk/Buffer.hpp"
#include "Az3D/VertexTypes.hpp"

namespace Az3D {

struct Bone {
    std::string name;
    int parentIndex = -1;
    glm::mat4 inverseBindMatrix; // from mesh space to bone space
    glm::mat4 localBindTransform; // T/R/S from glTF node
};

struct Skeleton {
    std::vector<Bone> bones;
    std::unordered_map<std::string, int> nameToIndex;
};

struct MeshSkinned {
    std::vector<VertexSkinned> vertices;
    std::vector<uint32_t> indices;
    Skeleton skeleton; // bones + hierarchy
    // optional: inverseBindMatrices etc.

    static SharedPtr<MeshSkinned> loadFromGLTF(const std::string& filePath);
    
    AzVulk::BufferData vertexBufferData;
    AzVulk::BufferData indexBufferData;
    void createDeviceBuffer(const AzVulk::Device* vkDevice);
};

}