#include "AzGame/DescriptorManager.hpp"
#include <stdexcept>

namespace AzGame {
    DescriptorManager::DescriptorManager(const VulkanDevice& device, VkDescriptorSetLayout descriptorSetLayout)
        : vulkanDevice(device), descriptorSetLayout(descriptorSetLayout) {
    }

    DescriptorManager::~DescriptorManager() {
        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(vulkanDevice.getLogicalDevice(), descriptorPool, nullptr);
        }
    }

    void DescriptorManager::createDescriptorPool(uint32_t maxFramesInFlight) {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = maxFramesInFlight;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = maxFramesInFlight;
        poolInfo.flags = 0; // Default behavior

        if (vkCreateDescriptorPool(vulkanDevice.getLogicalDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void DescriptorManager::createDescriptorSets(const std::vector<VkBuffer>& uniformBuffers, size_t uniformBufferSize) {
        std::vector<VkDescriptorSetLayout> layouts(uniformBuffers.size(), descriptorSetLayout);
        
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(uniformBuffers.size());
        allocInfo.pSetLayouts = layouts.data();

        descriptorSets.resize(uniformBuffers.size());
        if (vkAllocateDescriptorSets(vulkanDevice.getLogicalDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        // Configure each descriptor set
        for (size_t i = 0; i < uniformBuffers.size(); i++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = uniformBufferSize;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr; // Optional
            descriptorWrite.pTexelBufferView = nullptr; // Optional

            vkUpdateDescriptorSets(vulkanDevice.getLogicalDevice(), 1, &descriptorWrite, 0, nullptr);
        }
    }
}
