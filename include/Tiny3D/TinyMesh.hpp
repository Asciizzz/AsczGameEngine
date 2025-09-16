#pragma once

#include "Tiny3D/TinyVertex.hpp"
#include <string>

// Uniform mesh structure that holds raw data only
struct TinySubmesh {
    TinyVertexLayout vertexLayout;
    enum class IndexType {
        Uint8,
        Uint16,
        Uint32
    } indexType = IndexType::Uint32;

    // Raw byte data
    std::vector<uint8_t> vertexData;
    std::vector<uint8_t> indexData;
    int matIndex = -1;

    // Division is extremely slow, so cache these
    size_t indexCount = 0; 
    size_t vertexCount = 0;

    TinySubmesh() = default;

    TinySubmesh& setMaterial(int index) {
        matIndex = index;
        return *this;
    }

    template<typename VertexT>
    TinySubmesh& setVertices(const std::vector<VertexT>& verts) {
        vertexLayout = VertexT::getLayout();

        vertexData.resize(verts.size() * sizeof(VertexT));
        std::memcpy(vertexData.data(), verts.data(), vertexData.size());

        vertexCount = verts.size();
        return *this;
    }

    template<typename IndexT>
    TinySubmesh& setIndices(const std::vector<IndexT>& idx) {
        switch (sizeof(IndexT)) {
            case sizeof(uint8_t): indexType = IndexType::Uint8; break;
            case sizeof(uint16_t): indexType = IndexType::Uint16; break;
            case sizeof(uint32_t): indexType = IndexType::Uint32; break;
            default: indexType = IndexType::Uint32; break; // Fallback
        }

        indexData.resize(idx.size() * sizeof(IndexT));
        std::memcpy(indexData.data(), idx.data(), indexData.size());

        indexCount = idx.size();
        return *this;
    }

    template<typename VertexT, typename IndexT>
    static TinySubmesh create(const std::vector<VertexT>& verts,
                            const std::vector<IndexT>& idx,
                            int matIdx = -1) {
        TinySubmesh sm;
        sm.setVertices(verts).setIndices(idx).setMaterial(matIdx);
        return sm;
    }
};

struct TinySubmeshLOD { // WIP
    std::vector<TinySubmesh> levels;
    std::vector<float> distances;
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

    uint32_t hash = 0; // fnv1a hash of raw data
    uint32_t makeHash();

    enum class AddressMode {
        Repeat        = 0,
        ClampToEdge   = 1,
        ClampToBorder = 2
    } addressMode = AddressMode::Repeat;
};