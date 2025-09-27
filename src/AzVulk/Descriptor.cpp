#include "AzVulk/Descriptor.hpp"
#include "AzVulk/DataBuffer.hpp"
#include <stdexcept>

using namespace AzVulk;

void DescLayout::create(const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
    destroy();
    layout = create(lDevice, bindings);
}

VkDescriptorSetLayout DescLayout::create(VkDevice lDevice, const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(lDevice, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout");
    }

    return layout;
}

void DescLayout::destroy() {
    if (layout == VK_NULL_HANDLE) return;

    vkDestroyDescriptorSetLayout(lDevice, layout, nullptr);
    layout = VK_NULL_HANDLE;
}





void DescPool::create(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets) {
    destroy();
    pool = create(lDevice, poolSizes, maxSets);
}

VkDescriptorPool DescPool::create(VkDevice lDevice, const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets) {
    VkDescriptorPool pool = VK_NULL_HANDLE;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets       = maxSets;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();

    if (vkCreateDescriptorPool(lDevice, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool");
    }

    return pool;
}

void DescPool::destroy() {
    if (pool == VK_NULL_HANDLE) return;

    vkDestroyDescriptorPool(lDevice, pool, nullptr);
    pool = VK_NULL_HANDLE;
}



void DescSet::allocate(VkDescriptorPool pool, VkDescriptorSetLayout layout, uint32_t count) {
    cleanup();

    layoutOwned = false;
    poolOwned = false;

    this->layout = layout;
    this->pool = pool;

    allocate(count);
}

void DescSet::allocate(uint32_t count) {
    free(pool);

    sets.resize(count, VK_NULL_HANDLE);

    std::vector<VkDescriptorSetLayout> layouts(count, layout);
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = pool;
    allocInfo.descriptorSetCount = count;
    allocInfo.pSetLayouts        = layouts.data();

    if (vkAllocateDescriptorSets(lDevice, &allocInfo, sets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets");
    }
}

void DescSet::createOwnLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
    destroyLayout();
    layout = DescLayout::create(lDevice, bindings);
    layoutOwned = true;
}

void DescSet::createOwnPool(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets) {
    destroyPool();
    pool = DescPool::create(lDevice, poolSizes, maxSets);
    poolOwned = true;
}

void DescSet::free(VkDescriptorPool pool) {
    if (pool == VK_NULL_HANDLE) return;

    for (auto& set : sets) if (set != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(lDevice, pool, 1, &set);
    }

    sets.clear();
}

void DescSet::destroyPool() {
    if (poolOwned && pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(lDevice, pool, nullptr);
        pool = VK_NULL_HANDLE;
        poolOwned = false;
    }
}

void DescSet::destroyLayout() {
    if (layoutOwned && layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(lDevice, layout, nullptr);
        layout = VK_NULL_HANDLE;
        layoutOwned = false;
    }
}


DescWrite::DescWrite() {
    // Common defaults
    writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeSet.dstBinding = 0;
    writeSet.dstArrayElement = 0;
    writeSet.descriptorCount = 1;
}

DescWrite& DescWrite::setBufferInfo(std::vector<VkDescriptorBufferInfo> bufferInfo) {
    type = Type::Buffer;
    writeSet.pBufferInfo = bufferInfo.data();
    writeSet.pImageInfo = nullptr;
    writeSet.pTexelBufferView = nullptr;
    return *this;
}

DescWrite& DescWrite::setImageInfo(std::vector<VkDescriptorImageInfo> imageInfos) {
    type = Type::Image;
    writeSet.pImageInfo = imageInfos.data();
    writeSet.pBufferInfo = nullptr;
    writeSet.pTexelBufferView = nullptr;
    return *this;
}

DescWrite& DescWrite::setDstSet(VkDescriptorSet dstSet) {
    writeSet.dstSet = dstSet;
    return *this;
}
DescWrite& DescWrite::setDstBinding(uint32_t dstBinding) {
    writeSet.dstBinding = dstBinding;
    return *this;
}
DescWrite& DescWrite::setDstArrayElement(uint32_t dstArrayElement) {
    writeSet.dstArrayElement = dstArrayElement;
    return *this;
}
DescWrite& DescWrite::setDescCount(uint32_t count) {
    writeSet.descriptorCount = count;
    return *this;
}
DescWrite& DescWrite::setDescType(VkDescriptorType type) {
    writeSet.descriptorType = type;
    return *this;
}