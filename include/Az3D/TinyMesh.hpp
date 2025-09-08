#pragma once

#include "Az3D/VertexTypes.hpp"
#include <string>

// BVH structures (deprecated - to be reimplemented later)

struct BVHNode {
    glm::vec3 min;
    glm::vec3 max;

    /* Note:
    -1 children means leaf
    Leaf range is [l_leaf, r_leaf)
    */
    int l_child = -1;
    int r_child = -1;
    size_t l_leaf = 0;
    size_t r_leaf = 0;
};

struct HitInfo {
    bool hit = false;
    bool hasHit = false; // Keep both for compatibility
    size_t index = 0;
    uint32_t triangleIndex = 0; // Keep both for compatibility
    glm::vec3 prop = glm::vec3(-1.0f); // {u, v, t} (u, v are for barycentric coordinates, t is distance)
    glm::vec3 barycentric{}; // Keep both for compatibility
    float t = 1e30f; // Keep both for compatibility
    
    // Get the vertex and normal at the hit point
    glm::vec3 vrtx = glm::vec3(0.0f);
    glm::vec3 nrml = glm::vec3(0.0f);
    uint32_t materialId = 0;
};


// Uniform mesh structure that holds raw data only
struct TinySubmesh {
    std::vector<uint8_t> vertexData;
    std::vector<uint32_t> indices;
    int matIndex = -1;

    VertexLayout layout;

    TinySubmesh() = default;

    template<typename VertexT>
    TinySubmesh(const std::vector<VertexT>& verts, const std::vector<uint32_t>& idx, int matIdx = -1) {
        create(verts, idx, matIdx);
    }

    template<typename VertexT>
    void create(const std::vector<VertexT>& verts, const std::vector<uint32_t>& idx, int matIdx = -1) {
        layout = VertexT::getLayout();
        indices = idx;
        vertexData.resize(verts.size() * sizeof(VertexT));
        std::memcpy(vertexData.data(), verts.data(), vertexData.size());
        matIndex = matIdx;
    }

    size_t vertexCount() const { return vertexData.size() / layout.stride; }
};

struct TinyMaterial {
    bool shading = true;
    int toonLevel = 0;

    // Some debug values
    float normalBlend = 0.0f;
    float discardThreshold = 0.01f;

    int albTexture = -1;
    int nrmlTexture = -1;
};


// Raw texture data (no Vulkan handles)
struct TinyTexture {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<uint8_t> data;

    enum class AddressMode {
        Repeat        = 0,
        ClampToEdge   = 1,
        ClampToBorder = 2
    } addressMode = AddressMode::Repeat;
};