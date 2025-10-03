#include "TinyVK/Resource/DataBuffer.hpp"
#include "TinyVK/System/CmdBuffer.hpp"

#include <stdexcept>
#include <cstring>
#include <iostream>
#include <execution>


using namespace TinyVK;

// Move constructor
DataBuffer::DataBuffer(DataBuffer&& other) noexcept {
    device = other.device;

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
        device = other.device;

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

DataBuffer& DataBuffer::cleanup() {
    if (device == VK_NULL_HANDLE) return *this;

    if (buffer != VK_NULL_HANDLE) {
        if (mapped) {
            vkUnmapMemory(device, memory);
            mapped = nullptr;
        }

        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }

    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }

    return *this;
}

DataBuffer& DataBuffer::setDataSize(VkDeviceSize size) {
    dataSize = size;      return *this;
}
DataBuffer& DataBuffer::setUsageFlags(VkBufferUsageFlags flags) {
    usageFlags = flags;   return *this;
}
DataBuffer& DataBuffer::setMemPropFlags(VkMemoryPropertyFlags flags) {
    memPropFlags = flags; return *this;
}

DataBuffer& DataBuffer::createBuffer(const Device* deviceVK) {
    return createBuffer(deviceVK->device, deviceVK->pDevice);
}

DataBuffer& DataBuffer::createBuffer(VkDevice device, VkPhysicalDevice pDevice) {
    this->device = device; // Only need to save device
    VkPhysicalDevice physDev = pDevice;

    if (device == VK_NULL_HANDLE || pDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("DataBuffer: Vulkan device not initialized");
    }

    cleanup();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = dataSize;
    bufferInfo.usage = usageFlags;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = Device::findMemoryType(memRequirements.memoryTypeBits, memPropFlags, pDevice);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, memory, 0);
    return *this;
}

DataBuffer& DataBuffer::copyFrom(VkCommandBuffer cmdBuffer, VkBuffer srcBuffer, VkBufferCopy* copyRegion, uint32_t regionCount) {
    vkCmdCopyBuffer(cmdBuffer, srcBuffer, buffer, regionCount, copyRegion);
    return *this;
}

DataBuffer& DataBuffer::mapMemory() {
    if (!mapped) vkMapMemory(device, memory, 0, dataSize, 0, &mapped);
    return *this;
}

DataBuffer& DataBuffer::unmapMemory() {
    if (mapped) vkUnmapMemory(device, memory);
    mapped = nullptr;
    return *this;
}

DataBuffer& DataBuffer::uploadData(const void* data) {
    mapMemory();
    memcpy(mapped, data, dataSize);
    unmapMemory();
    return *this;
}

DataBuffer& DataBuffer::copyData(const void* data) {
    memcpy(mapped, data, dataSize);
    return *this;
}

DataBuffer& DataBuffer::mapAndCopy(const void* data) {
    mapMemory().copyData(data);
    return *this;
}

DataBuffer& DataBuffer::createDeviceLocalBuffer(const Device* deviceVK, const void* initialData) {
    // --- staging buffer (CPU visible) ---
    DataBuffer stagingBuffer;
    stagingBuffer
        .setDataSize(dataSize)
        .setUsageFlags(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
        .setMemPropFlags(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        .createBuffer(deviceVK)
        .uploadData(initialData);

    // Update usage flags to include transfer dst
    usageFlags = usageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    setUsageFlags(usageFlags);

    // Update memory properties to device local
    memPropFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    setMemPropFlags(memPropFlags);

    createBuffer(deviceVK);

    TempCmd copyCmd(deviceVK, deviceVK->transferPoolWrapper);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = dataSize;

    copyFrom(copyCmd.get(), stagingBuffer.get(), &copyRegion, 1);

    copyCmd.endAndSubmit();

    return *this;
}
