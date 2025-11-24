#pragma once

#include "tinyVertex.hpp"
#include "tinyType.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

#include <string>
#include <limits>

struct alignas(16) tinyMorph {
    glm::vec3 dPos;
    glm::vec3 dNrml;
    glm::vec3 dTang;
};

struct tinyMorphTarget {
    std::string name;
    std::vector<tinyMorph> deltas;
};

struct tinyMesh {
    tinyMesh() noexcept = default;

    tinyMesh(const tinyMesh&) = delete;
    tinyMesh& operator=(const tinyMesh&) = delete;

    tinyMesh(tinyMesh&&) noexcept = default;
    tinyMesh& operator=(tinyMesh&&) noexcept = default;

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

        // Compute AABB
        ABmin_ = glm::vec3(std::numeric_limits<float>::max());
        ABmax_ = glm::vec3(std::numeric_limits<float>::lowest());
        for (const auto& v : verts) {
            glm::vec3 pos = v.getPos();
            ABmin_ = glm::min(ABmin_, pos);
            ABmax_ = glm::max(ABmax_, pos);
        }

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

    void clearData() noexcept {
        vrtxData_.clear();
        indxData_.clear();
        mrphData_.clear();
    }

    size_t vrtxCount() const noexcept { return vrtxCount_; }
    size_t indxCount() const noexcept { return indxCount_; }
    size_t mrphCount() const noexcept { return mrphCount_; }
    size_t indxStride() const noexcept { return indxStride_; }

    std::vector<Part>& parts() noexcept { return parts_; }
    const std::vector<Part>& parts() const noexcept { return parts_; }

    explicit operator bool() const noexcept { return !vrtxData_.empty() && !indxData_.empty(); }

    const glm::vec3& ABmin() const noexcept { return ABmin_; }
    const glm::vec3& ABmax() const noexcept { return ABmax_; }

    std::string& mrphName(size_t targetIndex) noexcept {
        static std::string emptyStr;
        if (targetIndex >= mrphNames_.size()) return emptyStr;
        return mrphNames_[targetIndex];
    }
    const std::string& mrphName(size_t targetIndex) const noexcept {
        return const_cast<tinyMesh*>(this)->mrphName(targetIndex);
    }

// -----------------------------------------
// Vulkan API
// -----------------------------------------

    VkBuffer vrtxBuffer() const noexcept { return vrtxBuffer_; }
    VkBuffer indxBuffer() const noexcept { return indxBuffer_; }
    VkBuffer mrphBuffer() const noexcept { return mrphBuffer_; }
    VkIndexType indxType() const noexcept { return indxType_; }
    VkDescriptorSet mrphDsDescSet() const noexcept {
        return mrphCount() ? mrphDsDescSet_.get() : VK_NULL_HANDLE;
    }

    bool vkCreate(const tinyVk::Device* device, VkDescriptorSetLayout mrphLayout = VK_NULL_HANDLE, VkDescriptorPool mrphPool = VK_NULL_HANDLE) {
        dvk_ = device;

        using namespace tinyVk;

        vrtxBuffer_
            .setDataSize(vrtxData_.size())
            .setUsageFlags(BufferUsage::Vertex)
            .createDeviceLocalBuffer(dvk_, vrtxData_.data());

        indxBuffer_ 
            .setDataSize(indxData_.size())
            .setUsageFlags(BufferUsage::Index)
            .createDeviceLocalBuffer(dvk_, indxData_.data());

        switch (indxStride_) {
            case sizeof(uint8_t):  indxType_ = VK_INDEX_TYPE_UINT8;  break;
            case sizeof(uint16_t): indxType_ = VK_INDEX_TYPE_UINT16; break;
            case sizeof(uint32_t): indxType_ = VK_INDEX_TYPE_UINT32; break;
            default:               indxType_ = VK_INDEX_TYPE_MAX_ENUM; break;
        }

        if (mrphCount() == 0) {
            clearData();
            return true;
        }

        // Morph targets
        size_t mrphSize = mrphData_.size();
        if (mrphLayout != VK_NULL_HANDLE && mrphPool != VK_NULL_HANDLE && mrphSize > 0) {
            mrphBuffer_
                .setDataSize(mrphSize)
                .setUsageFlags(BufferUsage::Storage)
                .createDeviceLocalBuffer(dvk_, mrphData_.data());
            
            mrphDsDescSet_.allocate(dvk_->device, mrphPool, mrphLayout);

            DescWrite()
                .setDstSet(mrphDsDescSet_)
                .setType(DescType::StorageBuffer)
                .setDescCount(1)
                .setBufferInfo({ VkDescriptorBufferInfo{
                    mrphBuffer_,
                    0,
                    mrphSize
                } })
                .updateDescSets(dvk_->device);
        }

        clearData();
        return true;
    }

    // Static helpers
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

    // Vulkan parts
    const tinyVk::Device* dvk_ = nullptr;
    tinyVk::DataBuffer vrtxBuffer_;
    tinyVk::DataBuffer indxBuffer_;
    tinyVk::DataBuffer mrphBuffer_;
    tinyVk::DescSet mrphDsDescSet_;
    VkIndexType indxType_ = VK_INDEX_TYPE_UINT16;

    // AABB
    glm::vec3 ABmin_ = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 ABmax_ = glm::vec3(std::numeric_limits<float>::lowest());
};