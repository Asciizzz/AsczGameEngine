#pragma once

#include "AzVulk/Device.hpp"

namespace AzVulk {
    struct BufferData;

    struct DynamicDescriptor {
        DynamicDescriptor() = default;
        DynamicDescriptor(VkDevice device) : device(device) {}
        void init(VkDevice device) { this->device = device; }
        
        ~DynamicDescriptor();

        VkDevice device;

        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
        void createLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings);

        uint32_t maxSets = 0;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        std::vector<VkDescriptorPoolSize> poolSizes; // Contain both <Type> and <size>
        void createPool(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);

        std::vector<VkDescriptorSet> sets;
        const VkDescriptorSet getSet(uint32_t index) const { return sets[index]; }

        VkDescriptorSet set = VK_NULL_HANDLE;
        const VkDescriptorSet getSet() const { return set; }

        // Some really helpful functions
        static VkDescriptorSetLayoutBinding fastBinding(uint32_t binding,
                                                        VkDescriptorType type,
                                                        VkShaderStageFlags stageFlags,
                                                        uint32_t descriptorCount = 1);
    };
}
