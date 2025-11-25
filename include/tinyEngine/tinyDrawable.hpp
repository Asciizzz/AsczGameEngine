#pragma once

#include "tinyRegistry.hpp"

#include "tinyData/tinyMesh.hpp"
#include "tinyData/tinyMaterial.hpp"
#include "tinyData/tinyTexture.hpp"

namespace Mesh3D {
    struct Insta {
        glm::mat4 model = glm::mat4(1.0f); // Model matrix
        glm::vec4 other = glm::vec4(0.0f); // Additional data

        static VkVertexInputBindingDescription bindingDesc();
        static std::vector<VkVertexInputAttributeDescription> attrDescs();
    };

    struct InstaRange {
        tinyHandle mesh;
        uint32_t matIndex = 0;
        uint32_t offset = 0;
        uint32_t count = 0;
    };
}

struct ShaderGroup {
    ShaderGroup() noexcept {
        addMat(tinyMaterial());
    }

    tinyHandle shader;

    std::vector<tinyMaterial::Data> mats;
    std::unordered_map<tinyHandle, uint32_t> matMap; // tinyHandle to index in mats
    uint32_t matOffset = 0;

    uint32_t addMat(const tinyMaterial& mat) {
        mats.push_back(mat.data);
        return static_cast<uint32_t>(mats.size() - 1);
    }

    uint32_t getMatIndex(tinyHandle matHandle) const {
        auto it = matMap.find(matHandle);
        if (it != matMap.end()) {
            return it->second;
        }
        return UINT32_MAX; // Not found
    }

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

    tinyRegistry& fsr() noexcept { return *fsr_; }
    const tinyRegistry& fsr() const noexcept { return *fsr_; }

    VkBuffer instaBuffer() const noexcept { return instaBuffer_; }
    VkDescriptorSet matDescSet() const noexcept { return matDescSet_.get(); }

    const std::vector<ShaderGroup>& shaderGroups() const noexcept {
        return shaderGroups_;
    }

// --------------------------- Complex rendering --------------------------

    struct MeshEntry {
        tinyHandle mesh;
        glm::mat4 model = glm::mat4(1.0f);
        glm::vec4 other = glm::vec4(0.0f);
    };

    void startFrame(uint32_t frameIndex) noexcept;
    void submit(const MeshEntry& entry) noexcept;
    void finalize(uint32_t* totalInstances = nullptr, uint32_t* totalMaterials = nullptr);

private:
    uint32_t maxFramesInFlight_ = 2;
    uint32_t frameIndex_ = 0;

    tinyRegistry* fsr_ = nullptr;
    const tinyVk::Device* dvk_ = nullptr;

    // Perframe freshed list
    std::vector<ShaderGroup> shaderGroups_;
    std::unordered_map<tinyHandle, uint32_t> shaderIdxMap_;

    // Vulkan data
    tinyVk::DataBuffer instaBuffer_;

    tinyVk::DescSLayout matDescLayout_;
    tinyVk::DescPool matDescPool_;
    tinyVk::DescSet matDescSet_;
    tinyVk::DataBuffer matBuffer_;
};