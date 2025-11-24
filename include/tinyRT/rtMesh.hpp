#pragma once

#include "tinyRegistry.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "tinyData/tinyMesh.hpp"

namespace tinyRT {

struct MeshRender3D { // Node component
    tinyHandle mesh; // Point to mesh resource in registry
};

struct MeshStatic3D {
    static constexpr uint32_t MAX_INSTANCES = 100000; // 8mb - more than enough

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

// ---------------------------------------------------------------
    MeshStatic3D() = default;
    void init(const tinyVk::Device* dvk, const tinyPool<tinyMesh>* meshPool);

    MeshStatic3D(const MeshStatic3D&) = delete;
    MeshStatic3D& operator=(const MeshStatic3D&) = delete;

    MeshStatic3D(MeshStatic3D&&) noexcept = default;
    MeshStatic3D& operator=(MeshStatic3D&&) noexcept = default;

    void reset() {
        instaRanges_.clear();
        tempInstaMap_.clear();
    }

    struct Entry {
        tinyHandle mesh;
        glm::mat4 model;
        glm::vec4 other;
    };

    void submit(Entry entry) {
        tempInstaMap_[entry.mesh].push_back({ entry.model, entry.other });
    }

    size_t finalize() {
        instaRanges_.clear();

        size_t totalInstances = 0;
        for (const auto& [meshH, dataVec] : tempInstaMap_) {
            if (totalInstances + dataVec.size() > MAX_INSTANCES) break; // Should astronomically rarely happen

            Range range;
            range.mesh = meshH;
            range.offset = static_cast<uint32_t>(totalInstances);
            range.count = static_cast<uint32_t>(dataVec.size());
            instaRanges_.push_back(range);

            // Copy data
            size_t dataOffset = totalInstances * sizeof(Data);
            size_t dataSize = dataVec.size() * sizeof(Data);
            instaBuffer_.copyData(dataVec.data(), dataSize, dataOffset);

            totalInstances += dataVec.size();
        }

        tempInstaMap_.clear();
        return totalInstances;
    }

// Renderer need these
    VkBuffer instaBuffer() const noexcept { return instaBuffer_; }
    const std::vector<Range>& ranges() const noexcept { return instaRanges_; }

private:
    const tinyPool<tinyMesh>* meshPool_ = nullptr;

    tinyVk::DataBuffer instaBuffer_;
    std::vector<Range> instaRanges_;

    std::unordered_map<tinyHandle, std::vector<Data>> tempInstaMap_;
};

}

using rtMeshRender3D = tinyRT::MeshRender3D;
using rtMESHRD3D = tinyRT::MeshRender3D;

using rtMeshStatic3D = tinyRT::MeshStatic3D;