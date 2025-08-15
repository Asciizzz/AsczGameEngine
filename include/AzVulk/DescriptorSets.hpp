#pragma once

#include <array>

#include "AzVulk/Device.hpp"
#include "Helpers/Templates.hpp"

namespace Az3D {
    struct Texture;
    struct Material;
}

namespace AzVulk {
    struct BufferData;

    struct DynamicDescriptor {
        DynamicDescriptor() = default;
        DynamicDescriptor(VkDevice device) : device(device) {}
        void init(VkDevice device) { this->device = device; }
        
        ~DynamicDescriptor();

        VkDevice device;

        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
        void createSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings);

        uint32_t maxSets = 0;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        std::vector<VkDescriptorPoolSize> poolSizes; // Contain both <Type> and <size>
        void createPool(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);

        std::vector<VkDescriptorSet> sets;
        const VkDescriptorSet getSet(uint32_t index) const { return sets[index]; }

        // Some really helpful functions
        static VkDescriptorSetLayoutBinding fastBinding(uint32_t binding,
                                                        VkDescriptorType type,
                                                        VkShaderStageFlags stageFlags,
                                                        uint32_t descriptorCount = 1);


        // Template for relevant components
        void createGlobalDescriptorSets(
            const std::vector<BufferData>& uniformBufferDatas,
            size_t uniformBufferSize,
            VkImageView depthImageView,
            VkSampler depthSampler,
            uint32_t maxFramesInFlight
        );
    };


    class DescriptorManager {
    public:
        // Soon to remove the entire class
        DescriptorManager(VkDevice device);
        ~DescriptorManager();

        DescriptorManager(const DescriptorManager&) = delete;
        DescriptorManager& operator=(const DescriptorManager&) = delete;

        VkDevice device;

        // Soon to be 6 feet under
        DynamicDescriptor globalDynamicDescriptor;

        void createDescriptorSetLayouts(uint32_t maxFramesInFlight);
        void createDescriptorPools(uint32_t maxFramesInFlight);
    };
}
