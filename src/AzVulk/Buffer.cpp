#include "AzVulk/Buffer.hpp"
#include "AzVulk/Device.hpp"
#include "Az3D/Az3D.hpp"

#include <stdexcept>
#include <cstring>
#include <iostream>
#include <execution>


namespace AzVulk {

// Move constructor
BufferData::BufferData(BufferData&& other) noexcept {
    vkDevice = other.vkDevice;

    buffer = other.buffer;
    memory = other.memory;
    mapped = other.mapped;

    dataSize = other.dataSize;

    usageFlags = other.usageFlags;
    memoryFlags = other.memoryFlags;

    // Invalidate other's handles
    other.buffer = VK_NULL_HANDLE;
    other.memory = VK_NULL_HANDLE;
    other.mapped = nullptr;
}

// Move assignment
BufferData& BufferData::operator=(BufferData&& other) noexcept {
    if (this != &other) {
        cleanup();
        vkDevice = other.vkDevice;

        buffer = other.buffer;
        memory = other.memory;
        mapped = other.mapped;

        dataSize = other.dataSize;

        usageFlags = other.usageFlags;
        memoryFlags = other.memoryFlags;

        // Invalidate other's handles
        other.buffer = VK_NULL_HANDLE;
        other.memory = VK_NULL_HANDLE;
        other.mapped = nullptr;
    }
    return *this;
}

void BufferData::cleanup() {
    if (buffer != VK_NULL_HANDLE) {
        if (mapped) {
            vkUnmapMemory(vkDevice->lDevice, memory);
            mapped = nullptr;
        }

        vkDestroyBuffer(vkDevice->lDevice, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }

    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(vkDevice->lDevice, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}


void BufferData::setProperties(
    VkDeviceSize dataSize,
    VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags memoryFlags
) {
    this->dataSize = dataSize;
    this->usageFlags = usageFlags;
    this->memoryFlags = memoryFlags;
}

void BufferData::createBuffer() {
    VkDevice lDevice = vkDevice->lDevice;
    VkPhysicalDevice pDevice = vkDevice->pDevice;

    if (lDevice == VK_NULL_HANDLE || pDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("BufferData: Vulkan lDevice not initialized");
    }

    cleanup();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = dataSize;
    bufferInfo.usage = usageFlags;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(lDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(lDevice, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = Device::findMemoryType(memRequirements.memoryTypeBits, memoryFlags, pDevice);

    if (vkAllocateMemory(lDevice, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(lDevice, buffer, memory, 0);
}

}