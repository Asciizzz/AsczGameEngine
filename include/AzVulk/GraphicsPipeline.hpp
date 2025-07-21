#pragma once

#include <vulkan/vulkan.h>
#include "Buffer.hpp"

namespace AzVulk {
    class GraphicsPipeline {
    public:
        GraphicsPipeline(VkDevice device, VkExtent2D swapChainExtent, VkFormat swapChainImageFormat);
        ~GraphicsPipeline();

        // Delete copy constructor and assignment operator
        GraphicsPipeline(const GraphicsPipeline&) = delete;
        GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

        VkRenderPass getRenderPass() const { return renderPass; }
        VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }
        VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }
        VkPipeline getPipeline() const { return graphicsPipeline; }

        void recreate(VkExtent2D newExtent, VkFormat newFormat, VkFormat depthFormat);

    private:
        VkDevice device;
        VkExtent2D swapChainExtent;
        VkFormat swapChainImageFormat;

        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline graphicsPipeline = VK_NULL_HANDLE;

        void createRenderPass();
        void createDescriptorSetLayout();
        void createGraphicsPipeline();
        void cleanup();
    };
}
