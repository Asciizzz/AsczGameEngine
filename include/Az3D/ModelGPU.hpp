#pragma once

#include "Tiny3D/TinyModel.hpp"
#include "AzVulk/DataBuffer.hpp"

namespace Az3D {

struct SubmeshGPU {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkIndexType indexType;
};

struct ModelGPU {
    std::vector<SubmeshGPU> submeshes;

    // A single for all materials of the model
    VkDescriptorSet matDescSet = VK_NULL_HANDLE;

    static ModelGPU create(const TinyModel& model);
};

} // namespace Az3D