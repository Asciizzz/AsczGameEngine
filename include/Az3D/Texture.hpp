#pragma once

#include <string>
#include <vulkan/vulkan.h>

namespace Az3D {

// Vulkan texture resource (image + view + memory only)
struct Texture {
    std::string path;
    VkImage image         = VK_NULL_HANDLE;
    VkImageView view      = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

} // namespace Az3D
