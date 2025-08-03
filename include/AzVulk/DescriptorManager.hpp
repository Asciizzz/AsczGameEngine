#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <string>
#include "AzVulk/Device.hpp"

namespace Az3D {
    struct Texture;
}

namespace AzVulk {
    class DescriptorManager {
    public:
        DescriptorManager(const Device& device, VkDescriptorSetLayout descriptorSetLayout);
        ~DescriptorManager();

        
        DescriptorManager(const DescriptorManager&) = delete;
        DescriptorManager& operator=(const DescriptorManager&) = delete;

        void createDescriptorPool(uint32_t maxFramesInFlight, uint32_t maxMaterials = 10);
        void createDescriptorSetsForMaterial(const std::vector<VkBuffer>& uniformBuffers, size_t uniformBufferSize, 
                                            const Az3D::Texture* texture, size_t materialIndex);
        void createDescriptorSetsForMaterialWithUBO(const std::vector<VkBuffer>& uniformBuffers, size_t uniformBufferSize,
                                                    const Az3D::Texture* texture, VkBuffer materialUniformBuffer,
                                                    size_t materialIndex);
        
        VkDescriptorSet getDescriptorSet(uint32_t frameIndex, size_t materialIndex);
        
        const Device& vulkanDevice;
        VkDescriptorSetLayout descriptorSetLayout;
        
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

        // Material Index -> frame descriptor sets (one per frame in flight)
        std::unordered_map<size_t, std::vector<VkDescriptorSet>> materialDescriptorSets;
        uint32_t maxFramesInFlight = 2;
        uint32_t maxMaterials = 10;
    };
}
