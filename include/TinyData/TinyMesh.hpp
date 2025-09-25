#pragma once

#include "TinyData/TinyVertex.hpp"

struct TinySubmesh {
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
};

// Uniform mesh structure that holds raw data only
struct TinyMesh {
    TinyVertexLayout vertexLayout;
    enum class IndexType {
        Uint8,
        Uint16,
        Uint32
    } indexType = IndexType::Uint32;

    TinyMesh() = default;

    // Raw byte data
    std::vector<uint8_t> vertexData;
    std::vector<uint8_t> indexData;

    size_t vertexCount = 0;
    size_t indexCount = 0; 

    // Useful for reconstructing data (rarely needed)
    size_t indexStride = 0;

    std::vector<TinySubmesh> submeshes;

    TinyMesh& setSubmeshes(const std::vector<TinySubmesh>& subs);

    template<typename VertexT>
    TinyMesh& setVertices(const std::vector<VertexT>& verts) {
        vertexCount = verts.size();
        vertexLayout = VertexT::getLayout();

        vertexData.resize(vertexCount * sizeof(VertexT));
        std::memcpy(vertexData.data(), verts.data(), vertexData.size());

        return *this;
    }

    template<typename IndexT>
    TinyMesh& setIndices(const std::vector<IndexT>& idx) {
        indexCount = idx.size();
        indexStride = sizeof(IndexT);
        indexType = sizeToIndexType(indexStride);

        indexData.resize(indexCount * indexStride);
        std::memcpy(indexData.data(), idx.data(), indexData.size());

        return *this;
    }

    static IndexType sizeToIndexType(size_t size);
};