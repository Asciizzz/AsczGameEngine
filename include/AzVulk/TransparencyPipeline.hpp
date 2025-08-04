#pragma once

#include <vulkan/vulkan.h>
#include "AzVulk/Buffer.hpp"

namespace AzVulk {
    class TransparencyPipeline {
    public:
        TransparencyPipeline(VkDevice device, VkExtent2D swapChainExtent, 
                           VkRenderPass oitRenderPass, VkDescriptorSetLayout descriptorSetLayout);
        ~TransparencyPipeline();
        
        TransparencyPipeline(const TransparencyPipeline&) = delete;
        TransparencyPipeline& operator=(const TransparencyPipeline&) = delete;

        void recreate(VkExtent2D newExtent, VkRenderPass oitRenderPass);

        // Device context
        VkDevice device;
        VkExtent2D swapChainExtent;
        
        // Pipeline objects
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline transparencyPipeline = VK_NULL_HANDLE;

    private:
        void createTransparencyPipeline(VkRenderPass oitRenderPass, VkDescriptorSetLayout descriptorSetLayout);
        void cleanup();
    };
}
