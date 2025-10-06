#include "TinyVK/Resource/Descriptor.hpp"
#include "TinyVK/Resource/DataBuffer.hpp"
#include <stdexcept>

using namespace TinyVK;

DescLayout::DescLayout(DescLayout&& other) noexcept {
    device = other.device;
    layout = other.layout;

    other.device = VK_NULL_HANDLE;
    other.layout = VK_NULL_HANDLE;
}
DescLayout& DescLayout::operator=(DescLayout&& other) noexcept {
    if (this != &other) {
        device = other.device;
        layout = other.layout;

        other.device = VK_NULL_HANDLE;
        other.layout = VK_NULL_HANDLE;
    }
    return *this;
}

void DescLayout::create(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
    this->device = device;
    destroy();

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout");
    }
}

void DescLayout::destroy() {
    if (layout == VK_NULL_HANDLE) return;

    vkDestroyDescriptorSetLayout(device, layout, nullptr);
    layout = VK_NULL_HANDLE;
}




DescPool::DescPool(DescPool&& other) noexcept {
    device = other.device;
    pool = other.pool;

    other.device = VK_NULL_HANDLE;
    other.pool = VK_NULL_HANDLE;
}
DescPool& DescPool::operator=(DescPool&& other) noexcept {
    if (this != &other) {
        device = other.device;
        pool = other.pool;

        other.device = VK_NULL_HANDLE;
        other.pool = VK_NULL_HANDLE;
    }
    return *this;
}

void DescPool::create(VkDevice device, const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets) {
    this->device = device;
    destroy();

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets       = maxSets;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool");
    }
}

void DescPool::destroy() {
    if (pool == VK_NULL_HANDLE) return;

    vkDestroyDescriptorPool(device, pool, nullptr);
    pool = VK_NULL_HANDLE;
}


DescSet::DescSet(DescSet&& other) noexcept {
    device = other.device;
    set = other.set;
    layout = other.layout;
    pool = other.pool;

    other.device = VK_NULL_HANDLE;
    other.set = VK_NULL_HANDLE;
    other.layout = VK_NULL_HANDLE;
    other.pool = VK_NULL_HANDLE;
}
DescSet& DescSet::operator=(DescSet&& other) noexcept {
    if (this != &other) {
        device = other.device;
        set = other.set;
        layout = other.layout;
        pool = other.pool;

        other.device = VK_NULL_HANDLE;
        other.set = VK_NULL_HANDLE;
        other.layout = VK_NULL_HANDLE;
        other.pool = VK_NULL_HANDLE;
    }
    return *this;
}

void DescSet::allocate(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout) {
    this->pool = pool;
    this->layout = layout;
    this->device = device;

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &layout;
    allocInfo.pNext = nullptr;

    if (vkAllocateDescriptorSets(device, &allocInfo, &set) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets");
    }
}

void DescSet::free(VkDescriptorPool pool) {
    if (pool == VK_NULL_HANDLE) return;

    if (set != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device, pool, 1, &set);
        set = VK_NULL_HANDLE;
    }

    set = VK_NULL_HANDLE;
}

DescWrite& DescWrite::addWrite() {
    // Setting some default values
    VkWriteDescriptorSet newWrite{};
    newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    newWrite.pNext = nullptr;
    newWrite.dstBinding = 0;
    newWrite.dstArrayElement = 0;
    newWrite.descriptorCount = 1;

    writes.push_back(newWrite);
    writeCount = static_cast<uint32_t>(writes.size());
    
    // Add storage slots for this write
    imageInfoStorage.emplace_back();
    bufferInfoStorage.emplace_back();
    
    return *this;
}

VkWriteDescriptorSet& DescWrite::lastWrite() {
    if (writes.empty()) addWrite();
    return writes.back();
}

DescWrite& DescWrite::setBufferInfo(std::vector<VkDescriptorBufferInfo> bufferInfo) {
    // Store the buffer info for the current write (last one)  
    if (!writes.empty()) {
        size_t writeIndex = writes.size() - 1;
        if (writeIndex < bufferInfoStorage.size()) {
            bufferInfoStorage[writeIndex] = std::move(bufferInfo);
            lastWrite().pBufferInfo = bufferInfoStorage[writeIndex].data();
        }
    }
    lastWrite().pImageInfo = nullptr;
    lastWrite().pTexelBufferView = nullptr;
    return *this;
}

DescWrite& DescWrite::setImageInfo(std::vector<VkDescriptorImageInfo> imageInfos) {
    // Store the image info for the current write (last one)
    if (!writes.empty()) {
        size_t writeIndex = writes.size() - 1;
        if (writeIndex < imageInfoStorage.size()) {
            imageInfoStorage[writeIndex] = std::move(imageInfos);
            lastWrite().pImageInfo = imageInfoStorage[writeIndex].data();
        }
    }
    lastWrite().pBufferInfo = nullptr;
    lastWrite().pTexelBufferView = nullptr;
    return *this;
}

DescWrite& DescWrite::setDstSet(VkDescriptorSet dstSet) {
    lastWrite().dstSet = dstSet;
    return *this;
}
DescWrite& DescWrite::setDstBinding(uint32_t dstBinding) {
    lastWrite().dstBinding = dstBinding;
    return *this;
}
DescWrite& DescWrite::setDstArrayElement(uint32_t dstArrayElement) {
    lastWrite().dstArrayElement = dstArrayElement;
    return *this;
}
DescWrite& DescWrite::setDescCount(uint32_t count) {
    lastWrite().descriptorCount = count;
    return *this;
}
DescWrite& DescWrite::setType(VkDescriptorType type) {
    lastWrite().descriptorType = type;
    return *this;
}

DescWrite& DescWrite::updateDescSets(VkDevice device) {
    vkUpdateDescriptorSets(device, writeCount, writes.data(), 0, nullptr);
    return *this;
}