#pragma once

#include "TinyVK/System/Device.hpp"

namespace TinyVK {

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

    VkBuffer get() const { return buffer; }
    operator VkBuffer() const { return buffer; }


    VkDeviceMemory getMemory() const { return memory; }
    VkDeviceSize getDataSize() const { return dataSize; }
    VkBufferUsageFlags getUsageFlags() const { return usageFlags; }
    VkMemoryPropertyFlags getMemPropFlags() const { return memPropFlags; }


    DataBuffer& setDataSize(VkDeviceSize size);
    DataBuffer& setUsageFlags(VkBufferUsageFlags flags);
    DataBuffer& setMemPropFlags(VkMemoryPropertyFlags flags);

    DataBuffer& createBuffer(const Device* deviceVK);
    DataBuffer& createBuffer(VkDevice device, VkPhysicalDevice pDevice);

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

private:
    VkDevice device = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* mapped = nullptr;

    VkDeviceSize dataSize = 0;
    VkBufferUsageFlags usageFlags = 0;
    VkMemoryPropertyFlags memPropFlags = 0;
};

}
