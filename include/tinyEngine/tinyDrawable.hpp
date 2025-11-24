#pragma once

#include "tinyRegistry.hpp"

#include "tinyData/tinyMesh.hpp"

namespace Mesh3D {
    struct Static {
        glm::mat4 model = glm::mat4(1.0f); // Model matrix
        glm::vec4 other = glm::vec4(0.0f); // Additional data

        static VkVertexInputBindingDescription bindingDesc();
        static std::vector<VkVertexInputAttributeDescription> attrDescs();
    };

    struct InstaRange {
        tinyHandle mesh;
        uint32_t offset = 0;
        uint32_t count = 0;
    };
}


class tinyDrawable {
public:
    static constexpr size_t MAX_INSTANCES = 100000; // 8mb - more than enough
    static constexpr size_t MAX_BONES     = 102400; // 6.5mb ~ 400 instances x 256 bones - plenty

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
    const std::vector<Mesh3D::InstaRange>& instaRanges() const noexcept { return instaRanges_; }

// --------------------------- Complex rendering --------------------------

    struct MeshEntry {
        tinyHandle mesh;
        glm::mat4 model = glm::mat4(1.0f);
        glm::vec4 other = glm::vec4(0.0f);
    };

    void submit(const MeshEntry& entry) noexcept {
        instaMap_[entry.mesh].emplace_back(Mesh3D::Static{entry.model, entry.other});
    }

    size_t finalize();

private:
    uint32_t maxFramesInFlight_ = 2;
    tinyRegistry* fsr_ = nullptr;
    const tinyVk::Device* dvk_ = nullptr;

    // In the future we will group by shader
    std::unordered_map<tinyHandle, std::vector<Mesh3D::Static>> instaMap_;

    tinyVk::DataBuffer instaBuffer_;
    std::vector<Mesh3D::InstaRange> instaRanges_;
};