#include "AzVulk/DataBuffer.hpp"

#include <stdexcept>
#include <cstring>
#include <iostream>
#include <execution>


using namespace AzVulk;

// Move constructor
DataBuffer::DataBuffer(DataBuffer&& other) noexcept {
    lDevice = other.lDevice;

    buffer = other.buffer;
    memory = other.memory;
    mapped = other.mapped;

    dataSize = other.dataSize;

    usageFlags = other.usageFlags;
    memPropFlags = other.memPropFlags;

    // Invalidate other's handles
    other.buffer = VK_NULL_HANDLE;
    other.memory = VK_NULL_HANDLE;
    other.mapped = nullptr;
}

// Move assignment
DataBuffer& DataBuffer::operator=(DataBuffer&& other) noexcept {
    if (this != &other) {
        cleanup();
        lDevice = other.lDevice;

        buffer = other.buffer;
        memory = other.memory;
        mapped = other.mapped;

        dataSize = other.dataSize;

        usageFlags = other.usageFlags;
        memPropFlags = other.memPropFlags;

        // Invalidate other's handles
        other.buffer = VK_NULL_HANDLE;
        other.memory = VK_NULL_HANDLE;
        other.mapped = nullptr;
    }
    return *this;
}

void DataBuffer::cleanup() {
    if (buffer != VK_NULL_HANDLE) {
        if (mapped) {
            vkUnmapMemory(lDevice, memory);
            mapped = nullptr;
        }

        vkDestroyBuffer(lDevice, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }

    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(lDevice, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

DataBuffer& DataBuffer::setProperties(
    VkDeviceSize dataSize,
    VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags memPropFlags
) {
    this->dataSize = dataSize;
    this->usageFlags = usageFlags;
    this->memPropFlags = memPropFlags;
    return *this;
}

DataBuffer& DataBuffer::createBuffer(const Device* vkDevice) {
    return createBuffer(vkDevice->lDevice, vkDevice->pDevice);
}

DataBuffer& DataBuffer::createBuffer(VkDevice lDevice, VkPhysicalDevice pDevice) {
    this->lDevice = lDevice; // Only need to save lDevice
    VkPhysicalDevice physDev = pDevice;

    if (lDevice == VK_NULL_HANDLE || pDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("DataBuffer: Vulkan lDevice not initialized");
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
    allocInfo.memoryTypeIndex = Device::findMemoryType(memRequirements.memoryTypeBits, memPropFlags, pDevice);

    if (vkAllocateMemory(lDevice, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(lDevice, buffer, memory, 0);
    return *this;
}

DataBuffer& DataBuffer::copyFrom(VkCommandBuffer cmdBuffer, VkBuffer srcBuffer, VkBufferCopy* copyRegion, uint32_t regionCount) {
    vkCmdCopyBuffer(cmdBuffer, srcBuffer, buffer, regionCount, copyRegion);
    return *this;
}