#pragma once

#include "TinyData/TinyVertex.hpp"
#include "TinyExt/TinyHandle.hpp"
#include "TinyVK/Resource/DataBuffer.hpp"

#include <string>

struct TinySubmesh {
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    TinyHandle material;
};

// Uniform mesh structure that holds raw data only
struct TinyMesh {
    TinyMesh(const TinyMesh&) = delete;
    TinyMesh& operator=(const TinyMesh&) = delete;

    TinyMesh(TinyMesh&&) = default;
    TinyMesh& operator=(TinyMesh&&) = default;

    std::string name; // Mesh name from glTF
    
    TinyVertexLayout vertexLayout;
    enum class IndexType {
        Uint8  = 0,
        Uint16 = 1,
        Uint32 = 2
    } indexType = IndexType::Uint32; // You can use the value for comparison

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

    TinyMesh& addSubmesh(const TinySubmesh& sub);
    TinyMesh& writeSubmesh(const TinySubmesh& sub, uint32_t index);

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

    // Buffers for runtime use
    TinyVK::DataBuffer vertexBuffer;
    TinyVK::DataBuffer indexBuffer;
    VkIndexType vkIndexType = VK_INDEX_TYPE_UINT16;

    bool createBuffers(const TinyVK::Device* deviceVK);

    static VkIndexType tinyToVkIndexType(TinyMesh::IndexType type);
};