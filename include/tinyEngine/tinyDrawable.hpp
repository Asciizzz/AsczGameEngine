#pragma once

#include <unordered_set>

#include "tinyRegistry.hpp"

#include "tinyData/tinyMesh.hpp"
#include "tinyData/tinyMaterial.hpp"
#include "tinyData/tinyTexture.hpp"


class tinyDrawable {
public:
    static constexpr size_t MAX_INSTANCES = 100000; // 8mb - more than enough
    static constexpr size_t MAX_MATERIALS = 10000;  // 0.96mb - more than enough
    static constexpr size_t MAX_TEXTURES  = 65536;  // Hopefully not needing this many
    static constexpr size_t MAX_BONES     = 102400; // 6.5mb ~ 400 model x 256 bones x 64 bytes (mat4) - plenty
    static constexpr size_t MAX_MORPHS    = 256;    // Morph WEIGHTS, not Delta

    static std::vector<VkVertexInputBindingDescription> bindingDesc() noexcept;
    static std::vector<VkVertexInputAttributeDescription> attributeDescs() noexcept;

    struct CreateInfo {
        uint32_t maxFramesInFlight = 2;
        tinyRegistry* fsr = nullptr;
        const tinyVk::Device* dvk = nullptr;
    };

    struct Size_x1 { // Per-frame
        VkDeviceSize aligned = 0;   // Data aligned to min offset alignment
        VkDeviceSize unaligned = 0; // Actual data size to copy
    };

    struct InstaData {
        glm::mat4 model = glm::mat4(1.0f); // Model matrix
        glm::uvec4 other = glm::uvec4(0); // Additional data
    };

    struct MeshEntry {
        tinyHandle mesh;
        glm::mat4 model = glm::mat4(1.0f);

        std::vector<glm::mat4>* skins = nullptr;
    };

    struct MeshRange {
        tinyHandle mesh;
        std::vector<uint32_t> submeshes;

        uint32_t instaOffset = 0;
        uint32_t instaCount = 0;
    };

// ---------------------------------------------------------------

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

    VkDescriptorSet matDescSet() const noexcept { return matDescSet_; }
    VkDescriptorSetLayout matDescLayout() const noexcept { return matDescLayout_; }

    uint32_t matIndex(tinyHandle matHandle) const noexcept {
        auto it = matIdxMap_.find(matHandle);
        return (it != matIdxMap_.end()) ? it->second : 0;
    }

    VkDescriptorSet skinDescSet() const noexcept { return skinDescSet_; }
    VkDescriptorSetLayout skinDescLayout() const noexcept { return skinDescLayout_; }

    Size_x1 instaSize_x1() const noexcept { return instaSize_x1_; }
    Size_x1 matSize_x1() const noexcept { return matSize_x1_; }
    Size_x1 skinSize_x1() const noexcept { return skinSize_x1_; }

    inline VkDeviceSize instaOffset(uint32_t frameIndex) const noexcept { return frameIndex * instaSize_x1_.aligned; }
    inline VkDeviceSize matOffset(uint32_t frameIndex) const noexcept { return frameIndex * matSize_x1_.aligned; }
    inline VkDeviceSize skinOffset(uint32_t frameIndex) const noexcept { return frameIndex * skinSize_x1_.aligned; }

// --------------------------- Bacthking --------------------------

    void startFrame(uint32_t frameIndex) noexcept;
    void submit(const MeshEntry& entry) noexcept;
    void finalize();


    const std::unordered_map<tinyHandle, std::vector<MeshRange>>& shaderGroups() const noexcept {
        return shaderGroups_;
    }

private:
// Basic info
    uint32_t maxFramesInFlight_ = 2;
    uint32_t frameIndex_ = 0;

    tinyRegistry* fsr_ = nullptr;
    const tinyVk::Device* dvk_ = nullptr;

    // True implementation
    std::unordered_map<tinyHandle, std::vector<InstaData>> meshInstaMap_; // Mesh handle -> Instance data
    std::unordered_map<tinyHandle, std::vector<MeshRange>> shaderGroups_; // Shader handle -> mesh groups

    // Instances
    tinyVk::DataBuffer instaBuffer_;
    Size_x1 instaSize_x1_;

    // Materials
    tinyVk::DescSLayout matDescLayout_;
    tinyVk::DescPool matDescPool_;
    tinyVk::DescSet matDescSet_;
    tinyVk::DataBuffer matBuffer_;
    Size_x1 matSize_x1_;

    std::vector<tinyMaterial::Data> matData_;
    std::unordered_map<tinyHandle, uint32_t> matIdxMap_;

    // Textures
    tinyVk::DescSLayout texDescLayout_;
    tinyVk::DescPool texDescPool_;
    tinyVk::DescSet texDescSet_; // Dynamic descriptor set for textures
    std::unordered_set<tinyHandle> texInUse_;

    // Skinning
    tinyVk::DescSLayout skinDescLayout_;
    tinyVk::DescPool skinDescPool_;
    tinyVk::DescSet skinDescSet_;
    tinyVk::DataBuffer skinBuffer_;
    Size_x1 skinSize_x1_;
    uint32_t boneCount_ = 0;
};