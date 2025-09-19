#pragma once

#include "Tiny3D/TinyVertex.hpp"

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
        indexType = sizeToIndexType(sizeof(IndexT));

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

    TinySubmesh& setMaterial(int index);
    static IndexType sizeToIndexType(size_t size);
};
