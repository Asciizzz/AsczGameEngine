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

    TinyMesh& setSubmeshes(const std::vector<TinySubmesh>& subs);
    TinyMesh& addSubmesh(const TinySubmesh& sub);
    TinyMesh& writeSubmesh(const TinySubmesh& sub, uint32_t index);
    std::vector<TinySubmesh> submeshes;

    template<typename VertexT>
    TinyMesh& setVertices(const std::vector<VertexT>& verts) {
        vrtxCount_ = verts.size();
        vrtxLayout_ = VertexT::layout();

        vrtxData_.resize(vrtxCount_ * sizeof(VertexT));
        std::memcpy(vrtxData_.data(), verts.data(), vrtxData_.size());

        return *this;
    }

    template<typename IndexT>
    TinyMesh& setIndices(const std::vector<IndexT>& idx) {
        idxCount_ = idx.size();
        idxStride_ = sizeof(IndexT);
        idxType_ = sizeToIndexType(idxStride_);

        idxData_.resize(idxCount_ * idxStride_);
        std::memcpy(idxData_.data(), idx.data(), idxData_.size());

        return *this;
    }

    template<typename VertexT>
    VertexT* vrtxPtr() {
        if (vrtxData_.empty()) return nullptr;
        if (sizeof(VertexT) != vrtxLayout_.stride) return nullptr;
        return reinterpret_cast<VertexT*>(vrtxData_.data());
    }

    template<typename IndexT>
    IndexT* idxPtr() {
        if (idxData_.empty()) return nullptr;
        if (sizeof(IndexT) != idxStride_) return nullptr;
        return reinterpret_cast<IndexT*>(idxData_.data());
    }

    bool vkCreate(const TinyVK::Device* deviceVK);
    static VkIndexType sizeToIndexType(size_t size);

    size_t vrtxCount() const { return vrtxCount_; }
    size_t idxCount() const { return idxCount_; }
    const TinyVertex::Layout& vrtxLayout() const { return vrtxLayout_; }

    VkBuffer vrtxBuffer() const { return vrtxBuffer_; }
    VkBuffer idxBuffer() const { return idxBuffer_; }
    VkIndexType idxType() const { return idxType_; }

private:
    TinyVertex::Layout vrtxLayout_;
    std::vector<uint8_t> vrtxData_; // raw bytes
    size_t vrtxCount_ = 0;

    std::vector<uint8_t> idxData_; // raw bytes
    size_t idxCount_ = 0;
    size_t idxStride_ = 0;

    // Buffers for runtime use
    TinyVK::DataBuffer vrtxBuffer_;
    TinyVK::DataBuffer idxBuffer_;
    VkIndexType idxType_ = VK_INDEX_TYPE_UINT16;
};