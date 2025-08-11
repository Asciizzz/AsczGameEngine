#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <array>

#include "AzVulk/Device.hpp"

namespace Az3D {
    struct Texture;
}

namespace AzVulk {

    class DescriptorManager {
    public:
        DescriptorManager(const Device& device);
        ~DescriptorManager();

        DescriptorManager(const DescriptorManager&) = delete;
        DescriptorManager& operator=(const DescriptorManager&) = delete;

    // Create split descriptor set layouts (set 0: global UBO, set 1: material UBO+texture)
    void createDescriptorSetLayouts();
    void createDescriptorPools(uint32_t maxFramesInFlight, uint32_t maxMaterials = 10);

    void createGlobalDescriptorSets(const std::vector<VkBuffer>& uniformBuffers, size_t uniformBufferSize);
    void createMaterialDescriptorSets(const Az3D::Texture* texture, VkBuffer materialUniformBuffer, size_t materialIndex);

    void freeAllDescriptorSets();
    VkDescriptorSet getMaterialDescriptorSet(uint32_t frameIndex, size_t materialIndex);
    VkDescriptorSet getGlobalDescriptorSet(uint32_t frameIndex);

    const Device& vulkanDevice;
    // Descriptor set layouts
    VkDescriptorSetLayout globalDescriptorSetLayout = VK_NULL_HANDLE; // set 0
    VkDescriptorSetLayout materialDescriptorSetLayout = VK_NULL_HANDLE; // set 1

    // Descriptor pools
    VkDescriptorPool globalDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorPool materialDescriptorPool = VK_NULL_HANDLE;

    // Descriptor sets
    std::vector<VkDescriptorSet> globalDescriptorSets; // [frame]
    std::unordered_map<size_t, std::vector<VkDescriptorSet>> materialDescriptorSets; // materialIndex -> [frame]

    uint32_t maxFramesInFlight = 2;
    uint32_t maxMaterials = 10;
    };
}
