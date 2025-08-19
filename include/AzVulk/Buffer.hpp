#pragma once

#include "AzVulk/Device.hpp"

namespace AzVulk {

    struct BufferData {
        BufferData() = default;
        BufferData(const Device* vkDevice) : vkDevice(vkDevice) {}
        void initVulkanDevice(const Device* vkDevice) { this->vkDevice = vkDevice; }

        ~BufferData() { cleanup(); }
        void cleanup();

        // Non-copyable
        BufferData(const BufferData&) = delete;
        BufferData& operator=(const BufferData&) = delete;

        // Move constructor
        BufferData(BufferData&& other) noexcept;

        // Move assignment
        BufferData& operator=(BufferData&& other) noexcept;

        const Device* vkDevice;

        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;

        uint32_t resourceCount = 0;
        VkDeviceSize dataTypeSize = 0;
        VkDeviceSize totalDataSize = 0;

        VkBufferUsageFlags usageFlags = 0;
        VkMemoryPropertyFlags memoryFlags = 0;

        void createBuffer(
            size_t resourceCount, size_t dataTypeSize,
            VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryFlags
        );

        template<typename T>
        void uploadData(const std::vector<T>& data) {
            vkMapMemory(vkDevice->device, memory, 0, totalDataSize, 0, &mapped);
            memcpy(mapped, data.data(), sizeof(T) * data.size());
            vkUnmapMemory(vkDevice->device, memory);
            mapped = nullptr;
        }
        template<typename T>
        void uploadData(const T* data) {
            vkMapMemory(vkDevice->device, memory, 0, totalDataSize, 0, &mapped);
            memcpy(mapped, data, sizeof(T) * resourceCount);
            vkUnmapMemory(vkDevice->device, memory);
            mapped = nullptr;
        }

        void mappedData() {
            if (!mapped) vkMapMemory(vkDevice->device, memory, 0, totalDataSize, 0, &mapped);
        }

        template<typename T>
        void mappedData(const std::vector<T>& data) {
            mappedData();
            memcpy(mapped, data.data(), sizeof(T) * data.size());
        }

        template<typename T>
        void updateMapped(size_t index, const T& value) {
            static_cast<T*>(mapped)[index] = value;
        }
    };

}
