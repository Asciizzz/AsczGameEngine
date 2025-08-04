#pragma once

#include <vulkan/vulkan.h>

namespace AzVulk {
    class CompositePipeline {
    public:
        CompositePipeline(VkDevice device, VkExtent2D swapChainExtent, 
                         VkRenderPass mainRenderPass, VkDescriptorSetLayout compositeDescriptorSetLayout);
        ~CompositePipeline();
        
        CompositePipeline(const CompositePipeline&) = delete;
        CompositePipeline& operator=(const CompositePipeline&) = delete;

        void recreate(VkExtent2D newExtent, VkRenderPass mainRenderPass);

        // Device context
        VkDevice device;
        VkExtent2D swapChainExtent;
        
        // Pipeline objects
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline compositePipeline = VK_NULL_HANDLE;

    private:
        void createCompositePipeline(VkRenderPass mainRenderPass, VkDescriptorSetLayout compositeDescriptorSetLayout);
        void cleanup();
    };
}
