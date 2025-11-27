#pragma once

#include "tinyType.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

#include <glm/glm.hpp>

#include <string>
#include <limits>

namespace tinyMorph {
    struct alignas(16) Delta {
        glm::vec3 dPos;
        glm::vec3 dNrml;
        glm::vec3 dTang;
    };

    struct Target {
        std::string name;
        std::vector<Delta> deltas;
    };

    enum class Type : uint8_t {
        Rig,
        Color
    };
}

namespace tinyVertex {
    struct Static {
        // Pack into vec4 to guarantee alignment
        glm::vec4 pos_tu;
        glm::vec4 nrml_tv;
        glm::vec4 tang;

        // Some helpers
        Static& setPos (const glm::vec3& p) { pos_tu.x = p.x; pos_tu.y = p.y; pos_tu.z = p.z; return *this; }
        Static& setNrml(const glm::vec3& n) { nrml_tv.x = n.x; nrml_tv.y = n.y; nrml_tv.z = n.z; return *this; }
        Static& setUV  (const glm::vec2& uv){ pos_tu.w = uv.x; nrml_tv.w = uv.y; return *this; }
        Static& setTang(const glm::vec4& t) { tang = t; return *this; }
    };

    struct Rigged {
        glm::uvec4 boneIDs;
        glm::vec4 boneWs;
    };
};

struct tinyMesh {
    tinyMesh() noexcept = default;

    tinyMesh(const tinyMesh&) = delete;
    tinyMesh& operator=(const tinyMesh&) = delete;

    tinyMesh(tinyMesh&&) noexcept = default;
    tinyMesh& operator=(tinyMesh&&) noexcept = default;

// -----------------------------------------

    

    struct Submesh {
        uint32_t indxOffset = 0;
        uint32_t indxCount = 0;
        tinyHandle material;
    };

    tinyMesh& setVrtxStatic(const std::vector<tinyVertex::Static>& stas) {
        vrtxCount_ = stas.size();
        vstaticData_ = stas;
        return *this;
    }
    tinyMesh& setVrtxRigged(const std::vector<tinyVertex::Rigged>& rigs) {
        vriggedData_ = rigs;
        isRigged_ = rigs.size() > 0;
        return *this;
    }

    template<typename IndexT>
    tinyMesh& setIndxs(const std::vector<IndexT>& indices) {
        indxCount_ = indices.size();
        indxStride_ = sizeof(IndexT);
        indxType_ = strideToType(indxStride_);

        size_t byteSize = indxCount_ * indxStride_;
        indxRaw_.resize(byteSize);
        std::memcpy(indxRaw_.data(), indices.data(), byteSize);

        return *this;   
    }

    tinyMesh& addMrphTarget(const tinyMorph::Target& target) {
        mrphTargets_.push_back(target);
        return *this;
    }

    void vkCreate(const tinyVk::Device* device, VkDescriptorSetLayout mrphLayout = VK_NULL_HANDLE, VkDescriptorPool mrphPool = VK_NULL_HANDLE) {
        using namespace tinyVk;
        dvk_ = device;

        // Just in case of some weirdness
        vrtxCount_ = vstaticData_.size(); 
        indxCount_ = indxRaw_.size() / indxStride_;
        mrphCount_ = mrphTargets_.size();

    // Vertex buffers

        vstaticBuffer_
            .setDataSize(vstaticData_.size() * sizeof(tinyVertex::Static))
            .setUsageFlags(BufferUsage::Vertex)
            .createDeviceLocalBuffer(dvk_, vstaticData_.data());

        if (!isRigged_) vriggedData_.push_back(tinyVertex::Rigged()); // Dummy entry to avoid errors
        vriggedBuffer_
            .setDataSize(vriggedData_.size() * sizeof(tinyVertex::Rigged))
            .setUsageFlags(BufferUsage::Vertex)
            .createDeviceLocalBuffer(dvk_, vriggedData_.data());

    // Index buffer

        indxBuffer_ 
            .setDataSize(indxRaw_.size())
            .setUsageFlags(BufferUsage::Index)
            .createDeviceLocalBuffer(dvk_, indxRaw_.data());

    // Morph deltas

        if (mrphTargets_.empty() ||
            mrphLayout == VK_NULL_HANDLE ||
            mrphPool == VK_NULL_HANDLE) {
            clearData(); // Optional
            return; // Can't create morph data
        }

        hasMorphs_ = true;

        mrphDeltas_.resize(mrphCount_ * vrtxCount_);
        for (size_t i = 0; i < mrphCount_; ++i) {
            // Should not happen, hopefully
            assert(mrphTargets_[i].deltas.size() == vrtxCount_ && "Morph target delta count must match vertex count");

            std::memcpy(
                mrphDeltas_.data() + i * vrtxCount_,
                mrphTargets_[i].deltas.data(),
                vrtxCount_ * sizeof(tinyMorph::Delta)
            );
        }

        // Construct an empty weights array for CPU use
        mrphWeights_ = std::vector<float>(mrphCount_, 0.0f);

        mrphDltsBuffer_
            .setDataSize(mrphDeltas_.size() * sizeof(tinyMorph::Delta))
            .setUsageFlags(BufferUsage::Storage)
            .createDeviceLocalBuffer(dvk_, mrphDeltas_.data());

        // Write descriptor set
        mrphDltsDescSet_.allocate(dvk_->device, mrphPool, mrphLayout);
        DescWrite()
            .setDstSet(mrphDltsDescSet_)
            .setType(DescType::StorageBuffer)
            .setDescCount(1)
            .setBufferInfo({ VkDescriptorBufferInfo{
                mrphDltsBuffer_, 0, VK_WHOLE_SIZE
            } })
            .updateDescSets(dvk_->device);

        clearData(); // Optional
    }

    void clearData() noexcept {
        vstaticData_.clear();
        vriggedData_.clear();
        indxRaw_.clear();
        mrphDeltas_.clear();
        // Clear the targets but keep names
        for (auto& target : mrphTargets_) target.deltas.clear();
    }

// -----------------------------------------

    size_t vrtxCount() const noexcept { return vrtxCount_; }
    size_t indxCount() const noexcept { return indxCount_; }
    size_t indxStride() const noexcept { return indxStride_; }
    VkIndexType indxType() const noexcept { return indxType_; }

    size_t mrphCount() const noexcept { return mrphCount_; }
    bool isRigged() const noexcept { return isRigged_; }

    VkBuffer vstaticBuffer() const noexcept { return vstaticBuffer_; }
    VkBuffer vriggedBuffer() const noexcept { return vriggedBuffer_; }
    VkBuffer indxBuffer() const noexcept { return indxBuffer_; }

    VkBuffer mrphDltsBuffer() const noexcept { return mrphDltsBuffer_; }
    VkDescriptorSet mrphDltsDescSet() const noexcept { return mrphDltsDescSet_; }

    const std::vector<tinyVertex::Static>& vstaticData() const noexcept { return vstaticData_; }
    const std::vector<tinyVertex::Rigged>& vriggedData() const noexcept { return vriggedData_; }

    std::vector<Submesh>& submeshes() noexcept { return submeshes_; }
    const std::vector<Submesh>& submeshes() const noexcept { return submeshes_; }

    glm::vec3& ABmin() noexcept { return ABmin_; }
    glm::vec3& ABmax() noexcept { return ABmax_; }
    
    // No non-const access
    const std::vector<tinyMorph::Target>& mrphTargets() const noexcept { return mrphTargets_; }
    const std::vector<float>& mrphWeights() const noexcept { return mrphWeights_; }

private:
    std::vector<tinyVertex::Static> vstaticData_;
    std::vector<tinyVertex::Rigged> vriggedData_;
    std::vector<uint8_t>            indxRaw_;     // Raw bytes
    std::vector<Submesh>            submeshes_;

    std::vector<tinyMorph::Target>  mrphTargets_;
    std::vector<tinyMorph::Delta>   mrphDeltas_;  // Size: mrphCount_ * vrtxCount_

    std::vector<float> mrphWeights_; // A copyable weights for CPU use

// Vulkan stuff and shit idk
    const tinyVk::Device* dvk_ = nullptr;
    tinyVk::DataBuffer    vstaticBuffer_;
    tinyVk::DataBuffer    vriggedBuffer_;
    tinyVk::DataBuffer    indxBuffer_;

    // Morph Deltas (Morph Weights are dynamic for each instance)
    tinyVk::DataBuffer    mrphDltsBuffer_;
    tinyVk::DescSet       mrphDltsDescSet_;

    size_t      vrtxCount_  = 0;
    size_t      indxCount_  = 0;
    size_t      indxStride_ = 0;
    VkIndexType indxType_   = VK_INDEX_TYPE_UINT16;
    size_t      mrphCount_  = 0;

    bool        isRigged_  = false;
    bool        hasMorphs_ = false;

    // AABB (demo)
    glm::vec3 ABmin_ = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 ABmax_ = glm::vec3(std::numeric_limits<float>::lowest());

    static VkIndexType strideToType(size_t stride) noexcept {
        switch (stride) {
            case sizeof(uint8_t):  return VK_INDEX_TYPE_UINT8;
            case sizeof(uint16_t): return VK_INDEX_TYPE_UINT16;
            case sizeof(uint32_t): return VK_INDEX_TYPE_UINT32;
            default:               return VK_INDEX_TYPE_MAX_ENUM;
        }
    }
};