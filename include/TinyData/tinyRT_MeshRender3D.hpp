#pragma once

#include "tinyData/tinyMesh.hpp"
#include "tinyExt/tinyRegistry.hpp"
// #include "tinyVk/Resource/DataBuffer.hpp"
// #include "tinyVk/Resource/Descriptor.hpp"

namespace tinyVk {
    class DataBuffer;
    struct DescSet;
};

namespace tinyRT {

struct MeshRender3D {
    MeshRender3D() noexcept = default;
    void init(const tinyVk::Device* deviceVk, const tinyPool<tinyMeshVk>* meshPool, VkDescriptorSetLayout mrphWsDescSetLayout, VkDescriptorPool mrphWsDescPool, uint32_t maxFramesInFlight);

    MeshRender3D(const MeshRender3D&) = delete;
    MeshRender3D& operator=(const MeshRender3D&) = delete;

    MeshRender3D(MeshRender3D&&) noexcept = default;
    MeshRender3D& operator=(MeshRender3D&&) noexcept = default;

// -----------------------------------------

    MeshRender3D& setMesh(tinyHandle meshHandle);
    MeshRender3D& setSkeleNode(tinyHandle skeleNodeHandle);

    void copy(const MeshRender3D* other);

// -----------------------------------------

    float mrphWeight(size_t targetIndex) const noexcept {
        return targetIndex < mrphWeights_.size() ? mrphWeights_[targetIndex] : 0.0f;
    }

    bool setMrphWeight(size_t targetIndex, float weight) noexcept {
        if (targetIndex >= mrphWeights_.size()) return false;

        mrphWeights_[targetIndex] = weight;
        return true;
    }

// -----------------------------------------

    static void vkWrite(const tinyVk::Device* device, tinyVk::DataBuffer* buffer, tinyVk::DescSet* descSet, size_t maxFramesInFlight, size_t mrphCount);
    VkDescriptorSet mrphWsDescSet() const noexcept;
    uint32_t mrphWsDynamicOffset(uint32_t curFrame) const noexcept;
    void vkUpdate(uint32_t curFrame) noexcept;

// -----------------------------------------
    
    tinyHandle meshHandle() const noexcept { return meshHandle_; }
    tinyHandle skeleNodeHandle() const noexcept { return skeleNodeHandle_; }

    const tinyMeshVk* rMesh() const noexcept {
        return meshPool_ ? meshPool_->get(meshHandle_) : nullptr;
    }

    uint32_t mrphCount() const noexcept {
        const tinyMeshVk* mesh = rMesh();
        return mesh ? mesh->mrphCount() : 0;
    }

    bool hasMrph() const noexcept {
        return vkValid && mrphCount() > 0;
    }

private:
    tinyHandle meshHandle_;
    tinyHandle skeleNodeHandle_; // For skinning

    bool vkValid = false;
    const tinyPool<tinyMeshVk>* meshPool_ = nullptr;
    const tinyVk::Device* deviceVk_ = nullptr;
    uint32_t maxFramesInFlight_ = 2;

    // In the future there will be dedicated buffers and descriptors for morph targets
    std::vector<float> mrphWeights_; // 1 weight per morph target
    tinyVk::DataBuffer mrphWsBuffer_; // float
    tinyVk::DescSet mrphWsDescSet_;
};

} // namespace tinyRT

using tinyRT_MESHR = tinyRT::MeshRender3D;