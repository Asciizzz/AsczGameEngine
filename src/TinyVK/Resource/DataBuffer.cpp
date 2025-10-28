#include "TinyVK/Resource/DataBuffer.hpp"
#include "TinyVK/System/CmdBuffer.hpp"

#include <stdexcept>
#include <cassert>


using namespace TinyVK;

// Move constructor
DataBuffer::DataBuffer(DataBuffer&& other) noexcept {
    deviceVK_ = other.deviceVK_;

    buffer_ = other.buffer_;
    memory_ = other.memory_;
    mapped_ = other.mapped_;

    dataSize_ = other.dataSize_;

    usageFlags_ = other.usageFlags_;
    memPropFlags_ = other.memPropFlags_;

    // Invalidate other's handles
    other.buffer_ = VK_NULL_HANDLE;
    other.memory_ = VK_NULL_HANDLE;
    other.mapped_ = nullptr;
}

// Move assignment
DataBuffer& DataBuffer::operator=(DataBuffer&& other) noexcept {
    if (this != &other) {
        cleanup();
        deviceVK_ = other.deviceVK_;

        buffer_ = other.buffer_;
        memory_ = other.memory_;
        mapped_ = other.mapped_;

        dataSize_ = other.dataSize_;

        usageFlags_ = other.usageFlags_;
        memPropFlags_ = other.memPropFlags_;

        // Invalidate other's handles
        other.buffer_ = VK_NULL_HANDLE;
        other.memory_ = VK_NULL_HANDLE;
        other.mapped_ = nullptr;
    }
    return *this;
}

DataBuffer& DataBuffer::cleanup() {
    if (!deviceVK_) return *this;

    if (buffer_ != VK_NULL_HANDLE) {
        if (mapped_) {
            vkUnmapMemory(device(), memory_);
            mapped_ = nullptr;
        }

        vkDestroyBuffer(device(), buffer_, nullptr);
        buffer_ = VK_NULL_HANDLE;
    }

    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device(), memory_, nullptr);
        memory_ = VK_NULL_HANDLE;
    }

    return *this;
}

DataBuffer& DataBuffer::setDataSize(VkDeviceSize size) {
    dataSize_ = size;      return *this;
}
DataBuffer& DataBuffer::setUsageFlags(VkBufferUsageFlags flags) {
    usageFlags_ = flags;   return *this;
}
DataBuffer& DataBuffer::setMemPropFlags(VkMemoryPropertyFlags flags) {
    memPropFlags_ = flags; return *this;
}

DataBuffer& DataBuffer::createBuffer(const Device* deviceVK) {
    deviceVK_ = deviceVK;

    if (!deviceVK_) {
        throw std::runtime_error("DataBuffer: Vulkan deviceVK_ not initialized");
    }

    cleanup();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = dataSize_;
    bufferInfo.usage = usageFlags_;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device(), &bufferInfo, nullptr, &buffer_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer_!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device(), buffer_, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = Device::findMemoryType(memRequirements.memoryTypeBits, memPropFlags_, pDevice());

    if (vkAllocateMemory(device(), &allocInfo, nullptr, &memory_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer_ memory_!");
    }

    vkBindBufferMemory(device(), buffer_, memory_, 0);
    return *this;
}

DataBuffer& DataBuffer::copyFrom(VkCommandBuffer cmdBuffer, VkBuffer srcBuffer, VkBufferCopy* copyRegion, uint32_t regionCount) {
    vkCmdCopyBuffer(cmdBuffer, srcBuffer, buffer_, regionCount, copyRegion);
    return *this;
}

DataBuffer& DataBuffer::mapMemory() {
    if (!mapped_) vkMapMemory(device(), memory_, 0, dataSize_, 0, &mapped_);
    return *this;
}

DataBuffer& DataBuffer::unmapMemory() {
    if (mapped_) vkUnmapMemory(device(), memory_);
    mapped_ = nullptr;
    return *this;
}

DataBuffer& DataBuffer::uploadData(const void* data) {
    mapMemory();
    memcpy(mapped_, data, dataSize_);
    unmapMemory();
    return *this;
}

DataBuffer& DataBuffer::copyData(const void* data, size_t size, size_t offset) {
    if (!mapped_) throw std::runtime_error("Buffer not mapped_");
    if (size == 0) size = dataSize_ - offset; // prevent overflow
    assert(offset + size <= dataSize_ && "copyData out of bounds");

    std::memcpy(static_cast<char*>(mapped_) + offset, data, size);
    return *this;
}

DataBuffer& DataBuffer::mapAndCopy(const void* data, size_t size, size_t offset) {
    mapMemory().copyData(data, size, offset);
    return *this;
}

DataBuffer& DataBuffer::createDeviceLocalBuffer(const Device* deviceVK, const void* initialData) {
    // --- staging buffer_ (CPU visible) ---
    DataBuffer stagingBuffer;
    stagingBuffer
        .setDataSize(dataSize_)
        .setUsageFlags(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
        .setMemPropFlags(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        .createBuffer(deviceVK)
        .uploadData(initialData);

    // Update usage flags to include transfer dst
    usageFlags_ = usageFlags_ | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    setUsageFlags(usageFlags_);

    // Update memory_ properties to deviceVK_ local
    memPropFlags_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    setMemPropFlags(memPropFlags_);

    createBuffer(deviceVK);

    TempCmd copyCmd(deviceVK, deviceVK->transferPoolWrapper);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = dataSize_;

    copyFrom(copyCmd.get(), stagingBuffer.get(), &copyRegion, 1);

    copyCmd.endAndSubmit();

    return *this;
}
