#pragma once

#include "tinyData/tinyVertex.hpp"
#include "tinyExt/tinyHandle.hpp"
#include "tinyVK/Resource/DataBuffer.hpp"

#include <string>

struct tinySubmesh {
    uint32_t indexOffset = 0;
    uint32_t idxCount = 0;
    tinyHandle material;
};

// Uniform mesh structure that holds raw data only
struct tinyMesh {
    tinyMesh() = default;

    tinyMesh(const tinyMesh&) = delete;
    tinyMesh& operator=(const tinyMesh&) = delete;

    tinyMesh(tinyMesh&&) = default;
    tinyMesh& operator=(tinyMesh&&) = default;

    struct MorphTarget {
        std::string name;
        std::vector<uint8_t> vDeltaData; // raw bytes
    };

    std::string name; // Mesh name from glTF

    tinyMesh& setSubmeshes(const std::vector<tinySubmesh>& subs);
    tinyMesh& addSubmesh(const tinySubmesh& sub);
    tinyMesh& writeSubmesh(const tinySubmesh& sub, uint32_t index);

    template<typename VertexT>
    tinyMesh& setVertices(const std::vector<VertexT>& verts) {
        vrtxCount_ = verts.size();
        vrtxLayout_ = VertexT::layout();

        vrtxData_.resize(vrtxCount_ * sizeof(VertexT));
        std::memcpy(vrtxData_.data(), verts.data(), vrtxData_.size());

        return *this;
    }

    template<typename IndexT>
    tinyMesh& setIndices(const std::vector<IndexT>& idx) {
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

    bool vkCreate(const tinyVK::Device* deviceVK);
    static VkIndexType sizeToIndexType(size_t size);

    const tinyVertex::Layout& vrtxLayout() const { return vrtxLayout_; }
    size_t vrtxCount() const { return vrtxCount_; }
    size_t idxCount() const { return idxCount_; }

    VkBuffer vrtxBuffer() const { return vrtxBuffer_; }
    VkBuffer idxBuffer() const { return idxBuffer_; }
    VkIndexType idxType() const { return idxType_; }

    const std::vector<tinySubmesh>& submeshes() const { return submeshes_; }

private:
    tinyVertex::Layout vrtxLayout_;
    std::vector<uint8_t> vrtxData_; // raw bytes
    size_t vrtxCount_ = 0;

    std::vector<uint8_t> idxData_; // raw bytes
    size_t idxCount_ = 0;
    size_t idxStride_ = 0;

    std::vector<tinySubmesh> submeshes_;

    tinyVK::DataBuffer vrtxBuffer_;
    std::vector<tinyVK::DataBuffer> morphVrtxBuffers_;

    tinyVK::DataBuffer idxBuffer_;
    VkIndexType idxType_ = VK_INDEX_TYPE_UINT16;
};