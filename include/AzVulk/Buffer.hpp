#pragma once

#include "AzVulk/Device.hpp"

namespace AzVulk {

    struct BufferData {
        BufferData() = default;

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

        VkDeviceSize dataSize = 0;

        VkBufferUsageFlags usageFlags = 0;
        VkMemoryPropertyFlags memoryFlags = 0;

        void initVkDevice(const Device* vkDevice) {
            this->vkDevice = vkDevice;
        }

        void setProperties(
            VkDeviceSize dataSize,
            VkBufferUsageFlags usageFlags,
            VkMemoryPropertyFlags memoryFlags
        ) {
            this->dataSize = dataSize;
            this->usageFlags = usageFlags;
            this->memoryFlags = memoryFlags;
        }

        void createBuffer();

        template<typename T>
        void uploadData(const std::vector<T>& data) {
            vkMapMemory(vkDevice->device, memory, 0, dataSize, 0, &mapped);
            memcpy(mapped, data.data(), dataSize);
            vkUnmapMemory(vkDevice->device, memory);
            mapped = nullptr;
        }
        template<typename T>
        void uploadData(const T* data) {
            vkMapMemory(vkDevice->device, memory, 0, dataSize, 0, &mapped);
            memcpy(mapped, data, dataSize);
            vkUnmapMemory(vkDevice->device, memory);
            mapped = nullptr;
        }

        void mappedData() {
            if (!mapped) vkMapMemory(vkDevice->device, memory, 0, dataSize, 0, &mapped);
        }

        template<typename T>
        void mappedData(const std::vector<T>& data) {
            mappedData();
            memcpy(mapped, data.data(), dataSize);
        }

        template<typename T>
        void updateMapped(size_t index, const T& value) {
            static_cast<T*>(mapped)[index] = value;
        }
    };

}
