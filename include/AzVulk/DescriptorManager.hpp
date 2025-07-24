#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <string>
#include "AzVulk/VulkanDevice.hpp"

namespace Az3D {
    class Texture;
}

namespace AzVulk {
    class DescriptorManager {
    public:
        DescriptorManager(const VulkanDevice& device, VkDescriptorSetLayout descriptorSetLayout);
        ~DescriptorManager();

        // Delete copy constructor and assignment operator
        DescriptorManager(const DescriptorManager&) = delete;
        DescriptorManager& operator=(const DescriptorManager&) = delete;

        void createDescriptorPool(uint32_t maxFramesInFlight, uint32_t maxMaterials = 10);
        void createDescriptorSetsForMaterial(const std::vector<VkBuffer>& uniformBuffers, size_t uniformBufferSize, 
                                           const Az3D::Texture* texture, const std::string& materialId);
        
        VkDescriptorSet getDescriptorSet(uint32_t frameIndex, const std::string& materialId);
        
        const VulkanDevice& vulkanDevice;
        VkDescriptorSetLayout descriptorSetLayout;
        
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        
    private:
        // Material ID -> frame descriptor sets (one per frame in flight)
        std::unordered_map<std::string, std::vector<VkDescriptorSet>> materialDescriptorSets;
        uint32_t maxFramesInFlight = 2;
        uint32_t maxMaterials = 10;
    };
}
