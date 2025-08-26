#include "AzVulk/DescriptorSets.hpp"
#include "AzVulk/Buffer.hpp"
#include "Az3D/Az3D.hpp"
#include <stdexcept>

namespace AzVulk {

DynamicDescriptor::~DynamicDescriptor() {
    for (auto& set : sets) vkFreeDescriptorSets(device, pool, 1, &set);

    if (setLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
    if (pool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, pool, nullptr);
}

void DynamicDescriptor::createLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
    if (setLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
        setLayout = VK_NULL_HANDLE;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &setLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout");
    }
}

void DynamicDescriptor::createPool(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets) {
    if (pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, pool, nullptr);
        pool = VK_NULL_HANDLE;
    }

    this->poolSizes = poolSizes;


    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = maxSets;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool");
    }
}


// Helpful functions

VkDescriptorSetLayoutBinding DynamicDescriptor::fastBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t descriptorCount) {
    VkDescriptorSetLayoutBinding bindingInfo{};
    bindingInfo.binding = binding;
    bindingInfo.descriptorCount = descriptorCount;
    bindingInfo.descriptorType = type;
    bindingInfo.pImmutableSamplers = nullptr;
    bindingInfo.stageFlags = stageFlags;
    return bindingInfo;
}

}
