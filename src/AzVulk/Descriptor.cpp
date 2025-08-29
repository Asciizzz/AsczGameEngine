#include "AzVulk/Descriptor.hpp"
#include "AzVulk/Buffer.hpp"
#include "Az3D/Az3D.hpp"
#include <stdexcept>

namespace AzVulk {

DynamicDescriptor::~DynamicDescriptor() {
    for (auto& set : sets) vkFreeDescriptorSets(lDevice, pool, 1, &set);

    if (setLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(lDevice, setLayout, nullptr);
    if (pool != VK_NULL_HANDLE) vkDestroyDescriptorPool(lDevice, pool, nullptr);
}

void DynamicDescriptor::createLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
    if (setLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(lDevice, setLayout, nullptr);
        setLayout = VK_NULL_HANDLE;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(lDevice, &layoutInfo, nullptr, &setLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout");
    }
}

void DynamicDescriptor::referenceLayout(VkDescriptorSetLayout layout) {
    if (setLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(lDevice, setLayout, nullptr);
    }
    setLayout = layout;
}

void DynamicDescriptor::createPool(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets) {
    if (pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(lDevice, pool, nullptr);
        pool = VK_NULL_HANDLE;
    }

    this->poolSizes = poolSizes;


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


// Helpful functions

VkDescriptorSetLayoutBinding DynamicDescriptor::fastBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t descriptorCount) {
    VkDescriptorSetLayoutBinding bindingInfo{};
    bindingInfo.binding            = binding;
    bindingInfo.descriptorCount    = descriptorCount;
    bindingInfo.descriptorType     = type;
    bindingInfo.pImmutableSamplers = nullptr;
    bindingInfo.stageFlags         = stageFlags;
    return bindingInfo;
}



// Move constructor and assignment operator
DescLayout::DescLayout(DescLayout&& other) noexcept
    : lDevice(other.lDevice), layout(other.layout)
{
    other.lDevice = VK_NULL_HANDLE;
    other.layout = VK_NULL_HANDLE;
}

DescLayout& DescLayout::operator=(DescLayout&& other) noexcept
{
    if (this != &other) {
        cleanup(); // destroy current layout if valid

        lDevice = other.lDevice;
        layout = other.layout;

        other.lDevice = VK_NULL_HANDLE;
        other.layout = VK_NULL_HANDLE;
    }
    return *this;
}


void DescLayout::cleanup() {
    if (layout == VK_NULL_HANDLE) return;

    vkDestroyDescriptorSetLayout(lDevice, layout, nullptr);
    layout = VK_NULL_HANDLE;
}

void DescLayout::create(const VkDevice lDevice, const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
    this->lDevice = lDevice;
    cleanup();

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(lDevice, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout");
    }
}

void DescLayout::create(const VkDevice lDevice, const std::vector<BindInfo>& bindingInfos) {
    this->lDevice = lDevice;
    cleanup();

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

VkDescriptorSetLayoutBinding DescLayout::fastBinding(const BindInfo& bindInfo) {
    VkDescriptorSetLayoutBinding bindingInfo{};
    bindingInfo.binding            = bindInfo.binding;
    bindingInfo.descriptorCount    = bindInfo.descCount;
    bindingInfo.descriptorType     = bindInfo.type;
    bindingInfo.pImmutableSamplers = nullptr;
    bindingInfo.stageFlags         = bindInfo.stageFlags;
    return bindingInfo;
}





// Move constructor and assignment operator
DescPool::DescPool(DescPool&& other) noexcept
    : lDevice(other.lDevice), pool(other.pool) {
    other.lDevice = VK_NULL_HANDLE;
    other.pool = VK_NULL_HANDLE;
}

DescPool& DescPool::operator=(DescPool&& other) noexcept {
    if (this != &other) {
        cleanup(); // destroy current pool if valid

        lDevice = other.lDevice;
        pool = other.pool;

        other.lDevice = VK_NULL_HANDLE;
        other.pool = VK_NULL_HANDLE;
    }
    return *this;
}

void DescPool::cleanup() {
    if (pool == VK_NULL_HANDLE) return;

    vkDestroyDescriptorPool(lDevice, pool, nullptr);
    pool = VK_NULL_HANDLE;
}

void DescPool::create(const VkDevice lDevice, const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets) {
    this->lDevice = lDevice;

    cleanup();

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



void DescSets::cleanup() {
    for (auto& set : sets) {
        if (set == VK_NULL_HANDLE) continue;

        vkFreeDescriptorSets(lDevice, pool, 1, &set);
        set = VK_NULL_HANDLE;
    }
}

void DescSets::allocate(const VkDevice lDevice, const VkDescriptorPool pool, const VkDescriptorSetLayout layout, uint32_t count) {
    this->lDevice = lDevice;
    this->pool    = pool;
    this->layout  = layout;

    cleanup();

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




} // namespace AzVulk