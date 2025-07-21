#pragma once

#include <AzVulk/Device.hpp>

namespace AzVulk {

class Buffer {
public:
    Buffer(
        Device& device,
        VkDeviceSize size,
        uint32_t count,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkDeviceSize alignment=1
    );
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

private:
    Device& device;
    void* mapped = nullptr;

    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory bufferMemory = VK_NULL_HANDLE;

    VkDeviceSize bufferSize;
    uint32_t instanceCount;
    VkDeviceSize instanceSize;
    VkDeviceSize alignmentSize;
    VkBufferUsageFlags usageFlags;
    VkMemoryPropertyFlags memoryPropertyFlags;
};

} // namespace AzVulk