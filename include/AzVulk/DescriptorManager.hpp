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

    struct DynamicDescriptor {
        DynamicDescriptor(VkDevice device, uint32_t maxResources, uint32_t maxFramesInFlight) :
            device(device), maxResources(maxResources), maxFramesInFlight(maxFramesInFlight) {}
        DynamicDescriptor() = default;
        ~DynamicDescriptor();

        VkDevice device;
        uint32_t maxResources;
        uint32_t maxFramesInFlight;

        void init(VkDevice device) { this->device = device; }

        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
        void createSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings);

        VkDescriptorPool pool = VK_NULL_HANDLE;
        std::vector<VkDescriptorPoolSize> poolSizes;
        void createPool(const std::vector<VkDescriptorType>& types);

        std::vector<VkDescriptorSet> sets;
        const VkDescriptorSet getSet(uint32_t frameIndex) const { return sets[frameIndex]; }
        void moveSet(std::vector<VkDescriptorSet>& newLocation) { newLocation = std::move(sets); }

        std::unordered_map<size_t, std::vector<VkDescriptorSet>> mapSets;
        const VkDescriptorSet& getMapSet(size_t key, uint32_t frameIndex) const {
            return mapSets.at(key)[frameIndex];
        }
        void moveMapSet(size_t key, std::vector<VkDescriptorSet>& newLocation) { 
            newLocation = std::move(mapSets[key]); 
            mapSets.erase(key);
        }


        // Some really helpful functions
        static inline VkDescriptorSetLayoutBinding fastBinding( uint32_t binding,
                                                                VkDescriptorType type,
                                                                VkShaderStageFlags stageFlags,
                                                                uint32_t descriptorCount = 1);
    };


    class DescriptorManager {
    public:
        DescriptorManager(VkDevice device);
        ~DescriptorManager();

        DescriptorManager(const DescriptorManager&) = delete;
        DescriptorManager& operator=(const DescriptorManager&) = delete;

        VkDevice device;

        std::unordered_map<size_t, std::vector<VkDescriptorSet>> materialDescriptorSets; // materialIndex -> [frame]

        DynamicDescriptor materialDynamicDescriptor;
        DynamicDescriptor globalDynamicDescriptor;

        // Create split descriptor set layouts (set 0: global UBO, set 1: material UBO+texture)
        void createDescriptorSetLayouts(uint32_t maxFramesInFlight);
        void createDescriptorPools(uint32_t maxMaterials = 10);

        void createMaterialDescriptorSets(const Az3D::Texture* texture, VkBuffer materialUniformBuffer, size_t materialIndex);
        void createGlobalDescriptorSets(const std::vector<VkBuffer>& uniformBuffers, size_t uniformBufferSize);
    };
}
