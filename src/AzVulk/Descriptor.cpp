#include "AzVulk/Descriptor.hpp"
#include "AzVulk/DataBuffer.hpp"
#include "Az3D/Az3D.hpp"
#include <stdexcept>

using namespace AzVulk;

// Move constructor and assignment operator
DescWrapper::DescWrapper(DescWrapper&& other) noexcept
: lDevice(other.lDevice), pool(other.pool), layout(other.layout), sets(std::move(other.sets)) {
    other.lDevice = VK_NULL_HANDLE;

    other.pool = VK_NULL_HANDLE;
    other.poolOwned = false;

    other.layout = VK_NULL_HANDLE;
    other.layoutOwned = false;
}

DescWrapper& DescWrapper::operator=(DescWrapper&& other) noexcept {
    if (this != &other) {
        cleanupSets();
        cleanupPool();
        cleanupLayout();

        lDevice = other.lDevice;

        pool = other.pool;
        poolOwned = other.poolOwned;

        layout = other.layout;
        layoutOwned = other.layoutOwned;

        sets = std::move(other.sets);

        other.lDevice = VK_NULL_HANDLE;
        other.pool = VK_NULL_HANDLE;
        other.layout = VK_NULL_HANDLE;
    }
    return *this;
}



void DescWrapper::cleanupLayout() {
    if (layout == VK_NULL_HANDLE || !layoutOwned) return;

    vkDestroyDescriptorSetLayout(lDevice, layout, nullptr);
    layout = VK_NULL_HANDLE;
}
void DescWrapper::cleanupPool() {
    if (pool == VK_NULL_HANDLE || !poolOwned) return;

    vkDestroyDescriptorPool(lDevice, pool, nullptr);
    pool = VK_NULL_HANDLE;
}
void DescWrapper::cleanupSets() {
    for (auto& set : sets) {
        if (set == VK_NULL_HANDLE || pool == VK_NULL_HANDLE) continue;

        vkFreeDescriptorSets(lDevice, pool, 1, &set);
        set = VK_NULL_HANDLE;
    }
}
void DescWrapper::cleanup() {
    cleanupSets();
    cleanupPool();
    cleanupLayout();
}




void DescWrapper::createLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
    cleanupLayout();
    layoutOwned = true;
    layout = createLayout(lDevice, bindings);
}

void DescWrapper::createLayout(const std::vector<LayoutBind>& bindingInfos) {
    cleanupLayout();
    layoutOwned = true;
    layout = createLayout(lDevice, bindingInfos);
}

void DescWrapper::borrowLayout(VkDescriptorSetLayout newLayout) {
    cleanupLayout();
    layout = newLayout;
    layoutOwned = false;
}

VkDescriptorSetLayoutBinding DescWrapper::fastBinding(const LayoutBind& LayoutBind) {
    VkDescriptorSetLayoutBinding bindingInfo{};
    bindingInfo.binding            = LayoutBind.binding;
    bindingInfo.descriptorCount    = LayoutBind.descCount;
    bindingInfo.descriptorType     = LayoutBind.type;
    bindingInfo.pImmutableSamplers = nullptr;
    bindingInfo.stageFlags         = LayoutBind.stageFlags;
    return bindingInfo;
}




void DescWrapper::createPool(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets) {
    cleanupPool();
    poolOwned = true;
    pool = createPool(lDevice, poolSizes, maxSets);
}

void DescWrapper::borrowPool(VkDescriptorPool newPool) {
    cleanupPool();
    pool = newPool;
    poolOwned = false;
}



void DescWrapper::allocate(uint32_t count) {
    cleanupSets();

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

// Static methods for standalone layout/pool creation

VkDescriptorSetLayout DescWrapper::createLayout(VkDevice lDevice, const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
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

VkDescriptorSetLayout DescWrapper::createLayout(VkDevice lDevice, const std::vector<LayoutBind>& bindingInfos) {
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;

    std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
    for (const auto& bindingInfo : bindingInfos) {
        layoutBindings.push_back(fastBinding(bindingInfo));
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    layoutInfo.pBindings    = layoutBindings.data();

    if (vkCreateDescriptorSetLayout(lDevice, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout");
    }

    return layout;
}

VkDescriptorPool DescWrapper::createPool(VkDevice lDevice, const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets) {
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

VkDescriptorSet DescWrapper::createSet(VkDevice lDevice, VkDescriptorPool pool, VkDescriptorSetLayout layout) {
    VkDescriptorSet set = VK_NULL_HANDLE;

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &layout;

    if (vkAllocateDescriptorSets(lDevice, &allocInfo, &set) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor set");
    }

    return set;
}


void DescWrapper::destroyLayout(VkDevice lDevice, VkDescriptorSetLayout layout) {
    if (layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(lDevice, layout, nullptr);
    }
}
void DescWrapper::destroyPool(VkDevice lDevice, VkDescriptorPool pool) {
    if (pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(lDevice, pool, nullptr);
    }
}
void DescWrapper::freeSet(VkDevice lDevice, VkDescriptorPool pool, VkDescriptorSet set) {
    if (set != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(lDevice, pool, 1, &set);
    }
}