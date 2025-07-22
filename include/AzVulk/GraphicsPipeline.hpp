#pragma once

#include <vulkan/vulkan.h>
#include "Buffer.hpp"

namespace AzVulk {
    class GraphicsPipeline {
    public:
        GraphicsPipeline(VkDevice device, VkExtent2D swapChainExtent, VkFormat swapChainImageFormat,
                        const char* vertexShaderPath, const char* fragmentShaderPath, VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT);
        ~GraphicsPipeline();

        // Delete copy constructor and assignment operator
        GraphicsPipeline(const GraphicsPipeline&) = delete;
        GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

        VkRenderPass getRenderPass() const { return renderPass; }
        VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }
        VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }
        VkPipeline getPipeline() const { return graphicsPipeline; }

        void recreate(VkExtent2D newExtent, VkFormat newFormat, VkFormat depthFormat, VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT);

        const char* vertexShaderPath;
        const char* fragmentShaderPath;

    private:
        VkDevice device;
        VkExtent2D swapChainExtent;
        VkFormat swapChainImageFormat;
        VkSampleCountFlagBits msaaSamples;

        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline graphicsPipeline = VK_NULL_HANDLE;

        void createRenderPass();
        void createDescriptorSetLayout();
        void createGraphicsPipeline(const char* vertexShaderPath, const char* fragmentShaderPath);
        void cleanup();
    };
}
