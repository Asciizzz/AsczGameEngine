#pragma once

#include "tinyPool.hpp"

#include "tinyData/tinyMesh.hpp"

struct MeshStatic3D {
    static constexpr uint32_t MAX_INSTANCES = 100000; // 8mb - more than enough

    struct Data {
        glm::mat4 model = glm::mat4(1.0f); // Model matrix
        glm::vec4 other = glm::vec4(0.0f); // Additional data

        static VkVertexInputBindingDescription bindingDesc() {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 1;
            bindingDescription.stride = sizeof(Data);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
            return bindingDescription;
        }

        static std::vector<VkVertexInputAttributeDescription> attrDescs() {
            std::vector<VkVertexInputAttributeDescription> attribs(5);

            attribs[0] = {3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Data, model) + sizeof(glm::vec4) * 0};
            attribs[1] = {4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Data, model) + sizeof(glm::vec4) * 1};
            attribs[2] = {5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Data, model) + sizeof(glm::vec4) * 2};
            attribs[3] = {6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Data, model) + sizeof(glm::vec4) * 3};
            attribs[4] = {7, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Data, other)};
            return attribs;
        }
    };

    struct Range {
        tinyHandle mesh;
        uint32_t offset = 0;
        uint32_t count = 0;
    };

// ---------------------------------------------------------------
    MeshStatic3D() = default;
    void init(const tinyVk::Device* dvk, const tinyPool<tinyMesh>* meshPool) {
        using namespace tinyVk;

        meshPool_ = meshPool;

        // Create instance buffer with max size
        size_t bufferSize = MAX_INSTANCES * sizeof(MeshStatic3D::Data);
        instaBuffer_
            .setDataSize(bufferSize)
            .setUsageFlags(BufferUsage::Vertex)
            .setMemPropFlags(MemProp::HostVisibleAndCoherent)
            .createBuffer(dvk)
            .mapMemory();
    }

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