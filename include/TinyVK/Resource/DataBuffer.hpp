#pragma once

#include "tinyVk/System/Device.hpp"

namespace tinyVk {

struct BufferUsage {
    static constexpr VkBufferUsageFlags Vertex      = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    static constexpr VkBufferUsageFlags Index       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    static constexpr VkBufferUsageFlags Uniform     = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    static constexpr VkBufferUsageFlags Storage     = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    static constexpr VkBufferUsageFlags TransferSrc = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    static constexpr VkBufferUsageFlags TransferDst = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    static constexpr VkBufferUsageFlags Indirect    = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

    static constexpr VkBufferUsageFlags TransferSrcAndDst = TransferSrc | TransferDst;
};

class DataBuffer {
public:
    DataBuffer() = default;

    ~DataBuffer() { cleanup(); }
    DataBuffer& cleanup();

    // Non-copyable
    DataBuffer(const DataBuffer&) = delete;
    DataBuffer& operator=(const DataBuffer&) = delete;

    // Move constructor and assignment
    DataBuffer(DataBuffer&& other) noexcept;
    DataBuffer& operator=(DataBuffer&& other) noexcept;

    VkBuffer get() const { return buffer_; }
    operator VkBuffer() const { return buffer_; }


    VkDeviceMemory getMemory() const { return memory_; }
    VkDeviceSize getDataSize() const { return dataSize_; }
    VkBufferUsageFlags getUsageFlags() const { return usageFlags_; }
    VkMemoryPropertyFlags getMemPropFlags() const { return memPropFlags_; }


    DataBuffer& setDataSize(VkDeviceSize size);
    DataBuffer& setUsageFlags(VkBufferUsageFlags flags);
    DataBuffer& setMemPropFlags(VkMemoryPropertyFlags flags);

    DataBuffer& createBuffer(const Device* deviceVk);

    DataBuffer& copyFrom(VkCommandBuffer cmdBuffer, VkBuffer srcBuffer, VkBufferCopy* copyRegion, uint32_t regionCount);

    DataBuffer& mapMemory();
    DataBuffer& unmapMemory();

    DataBuffer& uploadData(const void* data);

    DataBuffer& copyData(const void* data, size_t size=0, size_t offset=0);

    DataBuffer& createDeviceLocalBuffer(const Device* deviceVk, const void* initialData);

private:
    const Device* deviceVk_ = nullptr;
    VkDevice device() const { return deviceVk_->device; }
    VkPhysicalDevice pDevice() const { return deviceVk_->pDevice; }

    VkBuffer buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    void* mapped_ = nullptr;

    VkDeviceSize dataSize_ = 0;
    VkBufferUsageFlags usageFlags_ = 0;
    VkMemoryPropertyFlags memPropFlags_ = 0;
};

}
