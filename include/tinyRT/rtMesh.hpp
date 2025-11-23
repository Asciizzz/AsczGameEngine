#pragma once

#include "tinyRegistry.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "tinyData/tinyMesh.hpp"

namespace tinyRT {

namespace Static3D {
    struct Data {
        glm::mat4 model = glm::mat4(1.0f); // Model matrix
        glm::vec4 other = glm::vec4(0.0f); // Additional data

        static VkVertexInputBindingDescription bindingDesc();
        static std::vector<VkVertexInputAttributeDescription> attrDescs();
    };

    struct Range {
        tinyHandle mesh;
        uint32_t offset = 0;
        uint32_t count = 0;
    };
};

struct MeshRender3D { // Node component
    tinyHandle mesh; // Point to mesh resource in registry
};

struct MeshStatic3D {
    static constexpr uint32_t MAX_INSTANCES = 100000; // 8mb - more than enough

// ---------------------------------------------------------------
    MeshStatic3D() = default;
    void init(const tinyVk::Device* deviceVk, const tinyPool<tinyMesh>* meshPool);

    MeshStatic3D(const MeshStatic3D&) = delete;
    MeshStatic3D& operator=(const MeshStatic3D&) = delete;

    MeshStatic3D(MeshStatic3D&&) noexcept = default;
    MeshStatic3D& operator=(MeshStatic3D&&) noexcept = default;

    VkBuffer instaBuffer() const { return instaBuffer_; } // Implicit conversion

    void reset() {
        instaData_.clear();
        instaRanges_.clear();
        tempInstaMap_.clear();
    }

private:
    const tinyPool<tinyMesh>* meshPool_ = nullptr;

    tinyVk::DataBuffer instaBuffer_;

    std::vector<Static3D::Data> instaData_;
    std::vector<Static3D::Range> instaRanges_;

    std::unordered_map<tinyHandle, std::vector<Static3D::Data>> tempInstaMap_;
};

}

using rtMeshRender3D = tinyRT::MeshRender3D;
using rtMESHRD3D = tinyRT::MeshRender3D;

using rtMeshStatic3D = tinyRT::MeshStatic3D;