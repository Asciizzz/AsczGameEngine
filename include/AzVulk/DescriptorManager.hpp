#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <array>
#include <memory>

#include "AzVulk/Device.hpp"

namespace Az3D {
    struct Texture;
    struct Material;
}

namespace AzVulk {

    struct DynamicDescriptor {
        DynamicDescriptor(VkDevice device, uint32_t maxFramesInFlight) :
            device(device), maxFramesInFlight(maxFramesInFlight) {}
        DynamicDescriptor() = default;
        ~DynamicDescriptor();

        VkDevice device;
        uint32_t maxResources;
        uint32_t maxFramesInFlight;

        void init(VkDevice device, uint32_t maxFramesInFlight=2) {
            this->device = device;
            this->maxFramesInFlight = maxFramesInFlight;
        }

        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
        void createSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings);

        VkDescriptorPool pool = VK_NULL_HANDLE;
        std::vector<VkDescriptorPoolSize> poolSizes;
        void createPool(uint32_t maxResources, const std::vector<VkDescriptorType>& types);

        std::vector<VkDescriptorSet> sets;
        const VkDescriptorSet getSet(uint32_t frameIndex) const { return sets[frameIndex]; }

        std::vector<std::vector<VkDescriptorSet>> manySets;
        const VkDescriptorSet getSet(uint32_t setsIndex, uint32_t frameIndex) const {
            return manySets[setsIndex][frameIndex];
        }

        // Some really helpful functions
        static VkDescriptorSetLayoutBinding fastBinding(uint32_t binding,
                                                        VkDescriptorType type,
                                                        VkShaderStageFlags stageFlags,
                                                        uint32_t descriptorCount = 1);


        // Template for relevant components
        void createGlobalDescriptorSets(
            const std::vector<VkBuffer>& uniformBuffers,
            size_t uniformBufferSize,
            VkImageView depthImageView,
            VkSampler depthSampler
        );
        void createMaterialDescriptorSets_LEGACY(const Az3D::Texture* texture, VkBuffer materialUniformBuffer, size_t materialIndex);

        void createMaterialDescriptorSets(
            const std::vector<std::shared_ptr<Az3D::Material>>& materials,
            const std::vector<VkBuffer>& materialUniformBuffers
        );
        void createTextureDescriptorSets(const std::vector<Az3D::Texture>& textures);
    };


    class DescriptorManager {
    public:
        DescriptorManager(VkDevice device);
        ~DescriptorManager();

        DescriptorManager(const DescriptorManager&) = delete;
        DescriptorManager& operator=(const DescriptorManager&) = delete;

        VkDevice device;

        DynamicDescriptor globalDynamicDescriptor;
        DynamicDescriptor materialDynamicDescriptor;
        DynamicDescriptor textureDynamicDescriptor;

        // Create split descriptor set layouts (set 0: global UBO, set 1: material UBO+texture)
        void createDescriptorSetLayouts(uint32_t maxFramesInFlight);
        void createDescriptorPools(uint32_t maxMaterials = 10, uint32_t maxTextures = 10);
    };
}
