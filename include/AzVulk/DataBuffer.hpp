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

    template<typename T>
    void uploadData(const T* data) {
        mapMemory();
        memcpy(mapped, data, dataSize);
        unmapMemory();
    }

    template<typename T>
    void copyData(const T* data) {
        memcpy(mapped, data, dataSize);
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

    template<typename T>
    void createDeviceLocalBuffer(const Device* deviceVK, const T* initialData) {
        // --- staging buffer (CPU visible) ---
        DataBuffer stagingBuffer;
        stagingBuffer
            .setDataSize(dataSize)
            .setUsageFlags(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
            .setMemPropFlags(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
            .createBuffer(deviceVK)
            .uploadData(initialData);

        // Update usage flags and create device local buffer
        usageFlags = usageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        setUsageFlags(usageFlags);
        createBuffer(deviceVK);

        TemporaryCommand copyCmd(deviceVK, deviceVK->transferPoolWrapper);

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = dataSize;

        copyFrom(copyCmd.get(), stagingBuffer.get(), &copyRegion, 1);

        copyCmd.endAndSubmit();
    }
};

}
