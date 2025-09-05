#pragma once

#include "AzVulk/Device.hpp"

namespace AzVulk {

    struct BufferData {
        BufferData() = default;
        BufferData(const Device* vkDevice) : vkDevice(vkDevice) {}

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

        VkBuffer get() const { return buffer; }

        bool hostVisible = true;
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
        );

        void createBuffer();

        template<typename T>
        void uploadData(const T* data) {
            mapMemory();
            memcpy(mapped, data, dataSize);
            unmapMemory();
        }

        void mapMemory() {
            if (!mapped) vkMapMemory(vkDevice->lDevice, memory, 0, dataSize, 0, &mapped);
        }
        void unmapMemory() {
            if (mapped) vkUnmapMemory(vkDevice->lDevice, memory);
            mapped = nullptr;
        }

        template<typename T>
        void copyData(const T* data) {
            memcpy(mapped, data, dataSize);
        }

        template<typename T>
        void mapAndCopy(const T* data) {
            if (!mapped) vkMapMemory(vkDevice->lDevice, memory, 0, dataSize, 0, &mapped);
            memcpy(mapped, data, dataSize);
        }

        template<typename T>
        void updateMapped(size_t index, const T& value) {
            static_cast<T*>(mapped)[index] = value;
        }

        template<typename T>
        void updateSingle(size_t index, const T& value) {
            mapMemory();
            static_cast<T*>(mapped)[index] = value;
            unmapMemory();
        }
    };

}
