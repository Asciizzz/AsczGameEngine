#pragma once

#include "AzVulk/Device.hpp"

namespace AzVulk {

    struct DataBuffer {
        DataBuffer() = default;

        ~DataBuffer() { cleanup(); }
        void cleanup();

        // Non-copyable
        DataBuffer(const DataBuffer&) = delete;
        DataBuffer& operator=(const DataBuffer&) = delete;

        // Move constructor and assignment
        DataBuffer(DataBuffer&& other) noexcept;
        DataBuffer& operator=(DataBuffer&& other) noexcept;

        VkDevice lDevice = VK_NULL_HANDLE;

        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;

        VkBuffer get() const { return buffer; }

        VkDeviceSize dataSize = 0;
        VkBufferUsageFlags usageFlags = 0;
        VkMemoryPropertyFlags memPropFlags = 0;

        DataBuffer& setProperties(
            VkDeviceSize dataSize,
            VkBufferUsageFlags usageFlags,
            VkMemoryPropertyFlags memPropFlags
        );

        DataBuffer& createBuffer(const Device* vkDevice);
        DataBuffer& createBuffer(VkDevice lDevice, VkPhysicalDevice pDevice);

        DataBuffer& copyFrom(VkCommandBuffer cmdBuffer, VkBuffer srcBuffer, VkBufferCopy* copyRegion, uint32_t regionCount);

        template<typename T>
        void uploadData(const T* data) {
            mapMemory();
            memcpy(mapped, data, dataSize);
            unmapMemory();
        }

        void mapMemory() {
            if (!mapped) vkMapMemory(lDevice, memory, 0, dataSize, 0, &mapped);
        }
        void unmapMemory() {
            if (mapped) vkUnmapMemory(lDevice, memory);
            mapped = nullptr;
        }

        template<typename T>
        DataBuffer& copyData(const T* data) {
            memcpy(mapped, data, dataSize);
            return *this;
        }

        template<typename T>
        void mapAndCopy(const T* data) {
            if (!mapped) vkMapMemory(lDevice, memory, 0, dataSize, 0, &mapped);
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
