#include "AzVulk/Descriptor.hpp"
#include "AzVulk/DataBuffer.hpp"
#include "Az3D/Az3D.hpp"
#include <stdexcept>

using namespace AzVulk;

// Move constructor and assignment operator
DescSets::DescSets(DescSets&& other) noexcept
: lDevice(other.lDevice), pool(other.pool), layout(other.layout), sets(std::move(other.sets)) {
    other.lDevice = VK_NULL_HANDLE;
    other.pool = VK_NULL_HANDLE;
    other.poolOwned = false;

    other.layout = VK_NULL_HANDLE;
    other.layoutOwned = false;
}

DescSets& DescSets::operator=(DescSets&& other) noexcept {
    if (this != &other) {
        cleanupLayout();
        cleanupPool();
        cleanupSets();

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



void DescSets::cleanupLayout() {
    if (layout == VK_NULL_HANDLE || !layoutOwned) return;

    vkDestroyDescriptorSetLayout(lDevice, layout, nullptr);
    layout = VK_NULL_HANDLE;
    layoutOwned = false;
}
void DescSets::cleanupPool() {
    if (pool == VK_NULL_HANDLE || !poolOwned) return;

    vkDestroyDescriptorPool(lDevice, pool, nullptr);
    pool = VK_NULL_HANDLE;
    poolOwned = false;
}
void DescSets::cleanupSets() {
    for (auto& set : sets) {
        if (set != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(lDevice, pool, 1, &set);
            set = VK_NULL_HANDLE;
        }
    }
}

DescSets::~DescSets() {
    cleanupSets();
    cleanupPool();
    cleanupLayout();
}




void DescSets::createLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
    cleanupLayout();

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(lDevice, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout");
    }
}

void DescSets::createLayout(const std::vector<LayoutBind>& bindingInfos) {
    cleanupLayout();

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
}

void DescSets::borrowLayout(VkDescriptorSetLayout newLayout) {
    cleanupLayout();
    layout = newLayout;
    layoutOwned = false;
}

VkDescriptorSetLayoutBinding DescSets::fastBinding(const LayoutBind& LayoutBind) {
    VkDescriptorSetLayoutBinding bindingInfo{};
    bindingInfo.binding            = LayoutBind.binding;
    bindingInfo.descriptorCount    = LayoutBind.descCount;
    bindingInfo.descriptorType     = LayoutBind.type;
    bindingInfo.pImmutableSamplers = nullptr;
    bindingInfo.stageFlags         = LayoutBind.stageFlags;
    return bindingInfo;
}




void DescSets::createPool(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets) {
    cleanupPool();

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets       = maxSets;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();

    if (vkCreateDescriptorPool(lDevice, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool");
    }
}

void DescSets::borrowPool(VkDescriptorPool newPool) {
    cleanupPool();
    pool = newPool;
    poolOwned = false;
}



void DescSets::allocate(uint32_t count) {
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