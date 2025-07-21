#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "VulkanDevice.hpp"

namespace AzGame {
    class DescriptorManager {
    public:
        DescriptorManager(const VulkanDevice& device, VkDescriptorSetLayout descriptorSetLayout);
        ~DescriptorManager();

        // Delete copy constructor and assignment operator
        DescriptorManager(const DescriptorManager&) = delete;
        DescriptorManager& operator=(const DescriptorManager&) = delete;

        void createDescriptorPool(uint32_t maxFramesInFlight);
        void createDescriptorSets(const std::vector<VkBuffer>& uniformBuffers, size_t uniformBufferSize);
        
        VkDescriptorSet getDescriptorSet(uint32_t frameIndex) const { 
            return descriptorSets[frameIndex]; 
        }
        
        VkDescriptorPool getDescriptorPool() const { return descriptorPool; }
        const std::vector<VkDescriptorSet>& getDescriptorSets() const { return descriptorSets; }

    private:
        const VulkanDevice& vulkanDevice;
        VkDescriptorSetLayout descriptorSetLayout;
        
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> descriptorSets;
    };
}
