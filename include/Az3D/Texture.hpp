#pragma once

#include <string>
#include <vulkan/vulkan.h>
#include <cstdint>

namespace Az3D {

// Raw texture data (no Vulkan handles)
struct Texture {
    int width = 0;
    int height = 0;
    int channels = 0;
    uint8_t* data = nullptr;

    // Free using delete[] since we allocate with new[]
    void free() {
        if (data) {
            delete[] data;
            data = nullptr;
        }
    }
    
    // Copy constructor and assignment to prevent double deletion
    Texture(const Texture& other) = delete;
    Texture& operator=(const Texture& other) = delete;
    
    // Move constructor and assignment
    Texture(Texture&& other) noexcept 
        : width(other.width), height(other.height), channels(other.channels), data(other.data) {
        other.data = nullptr;
    }
    
    Texture& operator=(Texture&& other) noexcept {
        if (this != &other) {
            free();
            width = other.width;
            height = other.height;
            channels = other.channels;
            data = other.data;
            other.data = nullptr;
        }
        return *this;
    }
    
    Texture() = default;
    ~Texture() { free(); }
};

// Vulkan texture resource (image + view + memory only)
struct TextureVK {
    VkImage image         = VK_NULL_HANDLE;
    VkImageView view      = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

} // namespace Az3D
