#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "AzVulk/VulkanDevice.hpp"

namespace AzVulk {
    class DescriptorManager {
    public:
        DescriptorManager(const VulkanDevice& device, VkDescriptorSetLayout descriptorSetLayout);
        ~DescriptorManager();

        // Delete copy constructor and assignment operator
        DescriptorManager(const DescriptorManager&) = delete;
        DescriptorManager& operator=(const DescriptorManager&) = delete;

        void createDescriptorPool(uint32_t maxFramesInFlight);
        void createDescriptorSets(const std::vector<VkBuffer>& uniformBuffers, size_t uniformBufferSize, 
                                 VkImageView textureImageView, VkSampler textureSampler);
        
        
        const VulkanDevice& vulkanDevice;
        VkDescriptorSetLayout descriptorSetLayout;
        
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> descriptorSets;
    };
}
