#pragma once

#include "Helpers/Templates.hpp"

#include <vulkan/vulkan.h>

namespace AzVulk {

struct CmdBuffer {
    VkDevice lDevice = VK_NULL_HANDLE;
    VkCommandPool cmdPool = VK_NULL_HANDLE;

    std::vector<VkCommandBuffer> cmdBuffers;

    CmdBuffer() = default;
    ~CmdBuffer();

    void create(VkDevice device, VkCommandPool pool, uint32_t count);

    // Operator [] for easy access
    template<typename T>
    const VkCommandBuffer& operator[](T index) const { return cmdBuffers[index]; }
};

};