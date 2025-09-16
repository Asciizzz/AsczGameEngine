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

    DataBuffer& setDataSize(VkDeviceSize size);
    DataBuffer& setUsageFlags(VkBufferUsageFlags flags);
    DataBuffer& setMemPropFlags(VkMemoryPropertyFlags flags);

    DataBuffer& createBuffer(const Device* deviceVK);
    DataBuffer& createBuffer(VkDevice lDevice, VkPhysicalDevice pDevice);

    DataBuffer& copyFrom(VkCommandBuffer cmdBuffer, VkBuffer srcBuffer, VkBufferCopy* copyRegion, uint32_t regionCount);

    DataBuffer& mapMemory();
    DataBuffer& unmapMemory();

    DataBuffer& uploadData(const void* data);

    DataBuffer& copyData(const void* data);
    DataBuffer& mapAndCopy(const void* data);

    DataBuffer& createDeviceLocalBuffer(const Device* deviceVK, const void* initialData);

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

    // For situations where we only need the buffer for copying
    static VkBuffer createDeviceLocalBuffer(const Device* deviceVK, const void* initialData,
                                            VkDeviceSize size, VkBufferUsageFlags usageFlags); // Only need the VkBuffer
};

}
