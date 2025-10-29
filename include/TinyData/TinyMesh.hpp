#pragma once

#include "tinyData/tinyVertex.hpp"
#include "tinyExt/tinyHandle.hpp"
#include "tinyVk/Resource/DataBuffer.hpp"

#include <string>

// Uniform mesh structure that holds raw data only
struct tinyMesh {
    std::string name; // Mesh name from glTF
    tinyMesh() noexcept = default;

    struct Part {
        uint32_t indxOffset = 0;
        uint32_t indxCount = 0;
        tinyHandle material;
    };

    struct MorphTarget {
        std::string name;
        std::vector<uint8_t> vDeltaData; // raw bytes
    };

// -----------------------------------------

    template<typename VertexT>
    tinyMesh& setVertices(const std::vector<VertexT>& verts) {
        vrtxCount_ = verts.size();
        vrtxLayout_ = VertexT::layout();

        vrtxData_.resize(vrtxCount_ * sizeof(VertexT));
        std::memcpy(vrtxData_.data(), verts.data(), vrtxData_.size());

        return *this;
    }

    template<typename IndexT>
    tinyMesh& setIndices(const std::vector<IndexT>& indx) {
        indxCount_ = indx.size();
        indxStride_ = sizeof(IndexT);

        indxData_.resize(indxCount_ * indxStride_);
        std::memcpy(indxData_.data(), indx.data(), indxData_.size());

        return *this;
    }

    tinyMesh& addPart(const Part& part) {
        parts_.push_back(part);
        return *this;
    }

// -----------------------------------------

    template<typename VertexT>
    VertexT* vrtxPtr() {
        if (vrtxData_.empty()) return nullptr;
        if (sizeof(VertexT) != vrtxLayout_.stride) return nullptr;
        return reinterpret_cast<VertexT*>(vrtxData_.data());
    }

    template<typename VertexT>
    const VertexT* vrtxPtr() const {
        return const_cast<tinyMesh*>(this)->vrtxPtr<VertexT>();
    }

    template<typename IndexT>
    IndexT* indxPtr() {
        if (indxData_.empty()) return nullptr;
        if (sizeof(IndexT) != indxStride_) return nullptr;
        return reinterpret_cast<IndexT*>(indxData_.data());
    }

    template<typename IndexT>
    const IndexT* indxPtr() const {
        return const_cast<tinyMesh*>(this)->indxPtr<IndexT>();
    }

    const tinyVertex::Layout& vrtxLayout() const noexcept { return vrtxLayout_; }
    const std::vector<uint8_t>& vrtxData() const noexcept { return vrtxData_; }
    const std::vector<uint8_t>& indxData() const noexcept { return indxData_; }
    size_t vrtxCount() const noexcept { return vrtxCount_; }
    size_t indxCount() const noexcept { return indxCount_; }
    size_t indxStride() const noexcept { return indxStride_; }

    std::vector<Part>& parts() noexcept { return parts_; }
    const std::vector<Part>& parts() const noexcept { return parts_; }

    bool valid() const noexcept {
        return !vrtxData_.empty() && !indxData_.empty();
    }

private:
    tinyVertex::Layout vrtxLayout_;
    std::vector<uint8_t> vrtxData_; // raw bytes
    size_t vrtxCount_ = 0;

    std::vector<uint8_t> indxData_; // raw bytes
    size_t indxCount_ = 0;
    size_t indxStride_ = 0;

    std::vector<Part> parts_;
};

struct tinyMeshVk {
    tinyMeshVk() noexcept = default;

    tinyMeshVk(const tinyMeshVk&) = delete;
    tinyMeshVk& operator=(const tinyMeshVk&) = delete;

    tinyMeshVk(tinyMeshVk&&) noexcept = default;
    tinyMeshVk& operator=(tinyMeshVk&&) noexcept = default;

// -----------------------------------------

    VkBuffer vrtxBuffer() const noexcept { return vrtxBuffer_; }
    VkBuffer indxBuffer() const noexcept { return indxBuffer_; }
    VkIndexType indxType() const noexcept { return indxType_; }

    tinyMesh& cpu() noexcept { return mesh_; }
    const tinyMesh& cpu() const noexcept { return mesh_; }

    std::vector<tinyMesh::Part>& parts() noexcept { return mesh_.parts(); }
    const std::vector<tinyMesh::Part>& parts() const noexcept { return mesh_.parts(); }

    const tinyVertex::Layout& vrtxLayout() const noexcept { return mesh_.vrtxLayout(); }

// -----------------------------------------

    bool create(tinyMesh&& mesh, const tinyVk::Device* deviceVk) {
        using namespace tinyVk;

        mesh_ = std::move(mesh);

        vrtxBuffer_
            .setDataSize(mesh_.vrtxData().size())
            .setUsageFlags(BufferUsage::Vertex)
            .createDeviceLocalBuffer(deviceVk, mesh_.vrtxData().data());

        indxBuffer_ 
            .setDataSize(mesh_.indxData().size())
            .setUsageFlags(BufferUsage::Index)
            .createDeviceLocalBuffer(deviceVk, mesh_.indxData().data());

        switch (mesh_.indxStride()) {
            case sizeof(uint8_t):  indxType_ = VK_INDEX_TYPE_UINT8;  break;
            case sizeof(uint16_t): indxType_ = VK_INDEX_TYPE_UINT16; break;
            case sizeof(uint32_t): indxType_ = VK_INDEX_TYPE_UINT32; break;
            default:               indxType_ = VK_INDEX_TYPE_MAX_ENUM; break; // (your problem lol)
        }

        return true;
    }

    void printInfo() {
        printf("tinyMeshVk Info:\n");
        printf("  Name: %s\n", mesh_.name.c_str());
        printf("  Vertex Count: %zu\n", mesh_.vrtxCount());
        printf("  Index Count: %zu\n", mesh_.indxCount());
        printf("  Index Type: ");
        switch (indxType_) {
            case VK_INDEX_TYPE_UINT8:  printf("UINT8\n");  break;
            case VK_INDEX_TYPE_UINT16: printf("UINT16\n"); break;
            case VK_INDEX_TYPE_UINT32: printf("UINT32\n"); break;
            default:                   printf("UNKNOWN\n"); break;
        }
        printf("  Parts:\n");
        for (size_t i = 0; i < mesh_.parts().size(); ++i) {
            const auto& part = mesh_.parts()[i];
            printf("    Offset=%u, Count=%u, Material Handle Index=%u\n",
                part.indxOffset, part.indxCount,
                part.material.valid() ? part.material.index : 0);
        }
    }

private:
    tinyMesh mesh_;

    tinyVk::DataBuffer vrtxBuffer_;
    tinyVk::DataBuffer indxBuffer_;
    VkIndexType indxType_ = VK_INDEX_TYPE_UINT16;
};