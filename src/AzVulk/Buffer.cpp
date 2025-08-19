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

        dataTypeSize = other.dataTypeSize;
        resourceCount = other.resourceCount;
        totalDataSize = other.totalDataSize;

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

            dataTypeSize = other.dataTypeSize;
            resourceCount = other.resourceCount;
            totalDataSize = other.totalDataSize;

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
                vkUnmapMemory(vkDevice->device, memory);
                mapped = nullptr;
            }

            vkDestroyBuffer(vkDevice->device, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
        }

        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(vkDevice->device, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    }

    void BufferData::createBuffer(
        size_t resourceCount, size_t dataTypeSize,
        VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryFlags
    ) {
        VkDevice device = vkDevice->device;
        VkPhysicalDevice physicalDevice = vkDevice->physicalDevice;

        if (device == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("BufferData: Vulkan device not initialized");
        }

        this->usageFlags = usageFlags;
        this->memoryFlags = memoryFlags;

        this->resourceCount = static_cast<uint32_t>(resourceCount);
        this->dataTypeSize = static_cast<VkDeviceSize>(dataTypeSize);
        this->totalDataSize = static_cast<VkDeviceSize>(dataTypeSize * resourceCount);

        cleanup();

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = totalDataSize;
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
        allocInfo.memoryTypeIndex = Device::findMemoryType(memRequirements.memoryTypeBits, memoryFlags, physicalDevice);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer memory!");
        }

        vkBindBufferMemory(device, buffer, memory, 0);
    }

}