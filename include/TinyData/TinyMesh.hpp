#pragma once

#include "TinyData/TinyVertex.hpp"
#include "TinyExt/TinyHandle.hpp"
#include "TinyVK/Resource/DataBuffer.hpp"

#include <string>

struct TinySubmesh {
    uint32_t indexOffset = 0;
    uint32_t idxCount = 0;
    TinyHandle material;
};

// Uniform mesh structure that holds raw data only
struct TinyMesh {
    TinyMesh() = default;

    TinyMesh(const TinyMesh&) = delete;
    TinyMesh& operator=(const TinyMesh&) = delete;

    TinyMesh(TinyMesh&&) = default;
    TinyMesh& operator=(TinyMesh&&) = default;

    struct MorphTarget {
        std::string name;
        std::vector<uint8_t> vDeltaData; // raw bytes
    };

    std::string name; // Mesh name from glTF

    TinyVertexLayout vrtxLayout;
    std::vector<uint8_t> vrtxData; // raw bytes
    size_t vrtxCount = 0;

    std::vector<uint8_t> idxData; // raw bytes
    size_t idxCount = 0;
    size_t idxStride = 0;

    std::vector<TinySubmesh> submeshes;

    TinyMesh& setSubmeshes(const std::vector<TinySubmesh>& subs);

    TinyMesh& addSubmesh(const TinySubmesh& sub);
    TinyMesh& writeSubmesh(const TinySubmesh& sub, uint32_t index);

    template<typename VertexT>
    TinyMesh& setVertices(const std::vector<VertexT>& verts) {
        vrtxCount = verts.size();
        vrtxLayout = VertexT::getLayout();

        vrtxData.resize(vrtxCount * sizeof(VertexT));
        std::memcpy(vrtxData.data(), verts.data(), vrtxData.size());

        return *this;
    }

    template<typename IndexT>
    TinyMesh& setIndices(const std::vector<IndexT>& idx) {
        idxCount = idx.size();
        idxStride = sizeof(IndexT);
        indexType = sizeToIndexType(idxStride);

        idxData.resize(idxCount * idxStride);
        std::memcpy(idxData.data(), idx.data(), idxData.size());

        return *this;
    }

    template<typename VertexT>
    VertexT* vPtr() {
        if (vrtxData.empty()) return nullptr;
        if (sizeof(VertexT) != vrtxLayout.stride) return nullptr;
        return reinterpret_cast<VertexT*>(vrtxData.data());
    }

    template<typename IndexT>
    IndexT* iPtr() {
        if (idxData.empty()) return nullptr;
        if (sizeof(IndexT) != idxStride) return nullptr;
        return reinterpret_cast<IndexT*>(idxData.data());
    }

    // Buffers for runtime use
    TinyVK::DataBuffer vertexBuffer;
    TinyVK::DataBuffer indexBuffer;
    VkIndexType indexType = VK_INDEX_TYPE_UINT16;

    bool vkCreate(const TinyVK::Device* deviceVK);
    static VkIndexType sizeToIndexType(size_t size);
};