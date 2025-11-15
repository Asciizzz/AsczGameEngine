#pragma once

#include "tinyData/tinyVertex.hpp"
#include "tinyHandle.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

#include <string>

struct alignas(16) tinyMorph {
    glm::vec3 dPos;
    glm::vec3 dNrml;
    glm::vec3 dTang;
};

struct tinyMorphTarget {
    std::string name;
    std::vector<tinyMorph> deltas;
};

// Uniform mesh structure that holds raw data only
struct tinyMesh {
    tinyMesh() noexcept = default;

    struct Part {
        uint32_t indxOffset = 0;
        uint32_t indxCount = 0;
        tinyHandle material;
    };

// -----------------------------------------

    template<typename VertexT>
    tinyMesh& setVrtxs(const std::vector<VertexT>& verts) {
        vrtxCount_ = verts.size();
        vrtxLayout_ = VertexT::layout();

        vrtxData_.resize(vrtxCount_ * sizeof(VertexT));
        std::memcpy(vrtxData_.data(), verts.data(), vrtxData_.size());

        return *this;
    }

    template<typename IndexT>
    tinyMesh& setIndxs(const std::vector<IndexT>& indxs) {
        indxCount_ = indxs.size();
        indxStride_ = sizeof(IndexT);

        indxData_.resize(indxCount_ * indxStride_);
        std::memcpy(indxData_.data(), indxs.data(), indxData_.size());

        return *this;
    }

    tinyMesh& addMrphTarget(const tinyMorphTarget& target) {
        // Mismatched vertex count, ignore
        if (target.deltas.size() != vrtxCount_) return *this;

        // Append name
        mrphNames_.push_back(target.name.empty() ? ("target_" + std::to_string(mrphCount_)) : target.name);

        // Append raw data
        size_t offset = mrphData_.size();
        size_t targetSize = vrtxCount_ * sizeof(tinyMorph);
        mrphData_.resize(offset + targetSize);

        std::memcpy(mrphData_.data() + offset, target.deltas.data(), targetSize);
        ++mrphCount_;

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
        if (indxData_.empty() || sizeof(IndexT) != indxStride_) return nullptr;
        return reinterpret_cast<IndexT*>(indxData_.data());
    }

    template<typename IndexT>
    const IndexT* indxPtr() const {
        return const_cast<tinyMesh*>(this)->indxPtr<IndexT>();
    }

    tinyMorph* mrphPtr(size_t targetIndex) {
        if (mrphData_.empty() || targetIndex >= mrphCount_) return nullptr;
        size_t offset = targetIndex * vrtxCount_ * sizeof(tinyMorph);
        return reinterpret_cast<tinyMorph*>(mrphData_.data() + offset);
    }

    const tinyMorph* mrphPtr(size_t targetIndex) const {
        return const_cast<tinyMesh*>(this)->mrphPtr(targetIndex);
    }

    const tinyVertex::Layout& vrtxLayout() const noexcept { return vrtxLayout_; }
    const std::vector<uint8_t>& vrtxData() const noexcept { return vrtxData_; }
    const std::vector<uint8_t>& indxData() const noexcept { return indxData_; }
    const std::vector<uint8_t>& mrphData() const noexcept { return mrphData_; }

    size_t vrtxCount() const noexcept { return vrtxCount_; }
    size_t indxCount() const noexcept { return indxCount_; }
    size_t mrphCount() const noexcept { return mrphCount_; }
    size_t indxStride() const noexcept { return indxStride_; }

    std::vector<Part>& parts() noexcept { return parts_; }
    const std::vector<Part>& parts() const noexcept { return parts_; }

    bool valid() const noexcept { return !vrtxData_.empty() && !indxData_.empty(); }

    std::string& mrphName(size_t targetIndex) noexcept {
        static std::string emptyStr;
        if (targetIndex >= mrphNames_.size()) return emptyStr;
        return mrphNames_[targetIndex];
    }
    const std::string& mrphName(size_t targetIndex) const noexcept {
        return const_cast<tinyMesh*>(this)->mrphName(targetIndex);
    }

private:
    tinyVertex::Layout vrtxLayout_;
    std::vector<uint8_t> vrtxData_; // raw bytes
    size_t vrtxCount_ = 0;

    std::vector<uint8_t> indxData_; // raw bytes
    size_t indxCount_ = 0;
    size_t indxStride_ = 0;

    // Optional
    std::vector<std::string> mrphNames_;
    std::vector<uint8_t> mrphData_; // raw bytes
    size_t mrphCount_ = 0;

    std::vector<Part> parts_;
};

struct tinyMeshVk {
    tinyMeshVk() noexcept = default;
    void init(const tinyVk::Device* deviceVk, VkDescriptorSetLayout mrphDsDescSetLayout, VkDescriptorPool mrphDsDescPool) {
        deviceVk_ = deviceVk;
        mrphDsDescSet_.allocate(deviceVk->device, mrphDsDescPool, mrphDsDescSetLayout);
    }

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

    size_t vrtxCount() const noexcept { return mesh_.vrtxCount(); }
    size_t indxCount() const noexcept { return mesh_.indxCount(); }
    size_t mrphCount() const noexcept { return mesh_.mrphCount(); }
    VkDescriptorSet mrphDsDescSet() const noexcept {
        return mesh_.mrphCount() ? mrphDsDescSet_.get() : VK_NULL_HANDLE;
    }

// -----------------------------------------

    tinyMeshVk& set(tinyMesh&& mesh) {
        mesh_ = std::move(mesh);
        return *this;
    }

    bool create() {
        using namespace tinyVk;

        vrtxBuffer_
            .setDataSize(mesh_.vrtxData().size())
            .setUsageFlags(BufferUsage::Vertex)
            .createDeviceLocalBuffer(deviceVk_, mesh_.vrtxData().data());

        indxBuffer_ 
            .setDataSize(mesh_.indxData().size())
            .setUsageFlags(BufferUsage::Index)
            .createDeviceLocalBuffer(deviceVk_, mesh_.indxData().data());

        switch (mesh_.indxStride()) {
            case sizeof(uint8_t):  indxType_ = VK_INDEX_TYPE_UINT8;  break;
            case sizeof(uint16_t): indxType_ = VK_INDEX_TYPE_UINT16; break;
            case sizeof(uint32_t): indxType_ = VK_INDEX_TYPE_UINT32; break;
            default:               indxType_ = VK_INDEX_TYPE_MAX_ENUM; break; // (your problem lol)
        }

        if (mesh_.mrphCount() == 0) return true; // No morph targets

        size_t mrphSize = mesh_.mrphData().size();
        mrphDsBuffer_
            .setDataSize(mrphSize)
            .setUsageFlags(BufferUsage::Storage)
            .createDeviceLocalBuffer(deviceVk_, mesh_.mrphData().data());

        DescWrite()
            .setDstSet(mrphDsDescSet_)
            .setType(DescType::StorageBuffer) // NOT DYNAMIC
            .setDescCount(1)
            .setBufferInfo({ VkDescriptorBufferInfo{
                mrphDsBuffer_,
                0,
                mrphSize
            } })
            .updateDescSets(deviceVk_->device);

        return true;
    }

    tinyMeshVk& createFrom(tinyMesh&& mesh) {
        return set(std::forward<tinyMesh>(mesh)).create(), *this;
    }

    // Both weights and deltas uses SSBO, so no need for 2 separate desc sets creation functions
    static tinyVk::DescSLayout createMrphDescSetLayout(VkDevice device, bool dynamic = true) {
        using namespace tinyVk;
        DescSLayout layout; layout.create(device, {
            {0, dynamic ? DescType::StorageBufferDynamic : DescType::StorageBuffer, 1, ShaderStage::Vertex, nullptr}
        });
        return layout;
    }

    static tinyVk::DescPool createMrphDescPool(VkDevice device, uint32_t maxMeshes, bool dynamic = true) {
        using namespace tinyVk;
        DescPool pool; pool.create(device, {
            {dynamic ? DescType::StorageBufferDynamic : DescType::StorageBuffer, maxMeshes}
        }, maxMeshes);
        return pool;
    }

private:
    const tinyVk::Device* deviceVk_;
    tinyMesh mesh_;

    tinyVk::DataBuffer vrtxBuffer_;
    tinyVk::DataBuffer indxBuffer_;

    tinyVk::DataBuffer mrphDsBuffer_;
    tinyVk::DescSet mrphDsDescSet_;

    VkIndexType indxType_ = VK_INDEX_TYPE_UINT16;
};