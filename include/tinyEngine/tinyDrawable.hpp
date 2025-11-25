#pragma once

#include "tinyRegistry.hpp"

#include "tinyData/tinyMesh.hpp"
#include "tinyData/tinyMaterial.hpp"
#include "tinyData/tinyTexture.hpp"

namespace Mesh3D {
    struct Insta {
        glm::mat4 model = glm::mat4(1.0f); // Model matrix
        glm::uvec4 other = glm::uvec4(0); // Additional data

        static VkVertexInputBindingDescription bindingDesc(uint32_t binding = 1);
        static std::vector<VkVertexInputAttributeDescription> attrDescs(uint32_t binding = 1, uint32_t locationOffset = 3);
    };

    struct InstaRange {
        tinyHandle mesh;
        uint32_t offset = 0;
        uint32_t count = 0;
    };
}

struct ShaderGroup {
    tinyHandle shader;

    std::unordered_map<tinyHandle, std::vector<Mesh3D::Insta>> instaMap;
    std::vector<Mesh3D::InstaRange> instaRanges;
};


class tinyDrawable {
public:
    static constexpr size_t MAX_INSTANCES = 100000; // 8mb - more than enough
    static constexpr size_t MAX_BONES     = 102400; // 6.5mb ~ 400 model x 256 bones x 64 bytes (mat4) - plenty
    static constexpr size_t MAX_MATERIALS = 10000;  // ~0.8-0.96mb, plenty

// ---------------------------------------------------------------

    struct CreateInfo {
        uint32_t maxFramesInFlight = 2;
        tinyRegistry* fsr = nullptr;
        const tinyVk::Device* dvk = nullptr;
    };

    tinyDrawable() noexcept = default;
    void init(const CreateInfo& info);

    tinyDrawable(const tinyDrawable&) = delete;
    tinyDrawable& operator=(const tinyDrawable&) = delete;

    tinyDrawable(tinyDrawable&&) noexcept = default;
    tinyDrawable& operator=(tinyDrawable&&) noexcept = default;

// --------------------------- Basic Getters ----------------------------

    uint32_t maxFramesInFlight() const noexcept { return maxFramesInFlight_; }
    uint32_t frameIndex() const noexcept { return frameIndex_; }

    tinyRegistry& fsr() noexcept { return *fsr_; }
    const tinyRegistry& fsr() const noexcept { return *fsr_; }

    VkBuffer instaBuffer() const noexcept { return instaBuffer_; }
    VkDescriptorSet matDescSet() const noexcept { return matDescSet_.get(); }

    const std::vector<ShaderGroup>& shaderGroups() const noexcept { return shaderGroups_; }

    uint32_t matIndex(tinyHandle matHandle) const noexcept {
        auto it = matIdxMap_.find(matHandle);
        return (it != matIdxMap_.end()) ? it->second : UINT32_MAX;
    }

    struct Size_x1 { // Per-frame
        VkDeviceSize aligned = 0;   // Data aligned to min offset alignment
        VkDeviceSize unaligned = 0; // Actual data size to copy
    };

    Size_x1 instaSize_x1() const noexcept { return instaSize_x1_; }
    Size_x1 matSize_x1() const noexcept { return matSize_x1_; }

    inline VkDeviceSize instaOffset(uint32_t frameIndex) const noexcept {
        return frameIndex * instaSize_x1_.aligned;
    }

    inline VkDeviceSize matOffset(uint32_t frameIndex) const noexcept {
        return frameIndex * matSize_x1_.aligned;
    }

// --------------------------- Bacthking --------------------------

    struct MeshEntry {
        tinyHandle mesh;
        tinyHandle mat;
        tinyHandle shader;
        glm::mat4 model = glm::mat4(1.0f);
    };

    void startFrame(uint32_t frameIndex) noexcept;
    void submit(const MeshEntry& entry) noexcept;
    void finalize(uint32_t* totalInstances = nullptr, uint32_t* totalMaterials = nullptr);

private:
// Basic info
    uint32_t maxFramesInFlight_ = 2;
    uint32_t frameIndex_ = 0;

    tinyRegistry* fsr_ = nullptr;
    const tinyVk::Device* dvk_ = nullptr;

    // Perframe freshed list
    std::vector<ShaderGroup> shaderGroups_;
    std::unordered_map<tinyHandle, uint32_t> shaderIdxMap_;

// Vulkan data

    tinyVk::DataBuffer instaBuffer_;
    Size_x1 instaSize_x1_;

    tinyVk::DescSLayout matDescLayout_;
    tinyVk::DescPool matDescPool_;
    tinyVk::DescSet matDescSet_;

    tinyVk::DataBuffer matBuffer_;
    Size_x1 matSize_x1_;

    std::vector<tinyMaterial::Data> matData_;
    std::unordered_map<tinyHandle, uint32_t> matIdxMap_;
};