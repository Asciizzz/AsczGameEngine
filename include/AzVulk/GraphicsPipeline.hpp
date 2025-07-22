#pragma once

#include <vulkan/vulkan.h>
#include "AzVulk/Buffer.hpp"

namespace AzVulk {
    class GraphicsPipeline {
    public:
        GraphicsPipeline(VkDevice device, VkExtent2D swapChainExtent, VkFormat swapChainImageFormat,
                        const char* vertexShaderPath, const char* fragmentShaderPath, VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT);
        ~GraphicsPipeline();

        // Delete copy constructor and assignment operator
        GraphicsPipeline(const GraphicsPipeline&) = delete;
        GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

        void recreate(VkExtent2D newExtent, VkFormat newFormat, VkFormat depthFormat, VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT);

        
        const char* vertexShaderPath;
        const char* fragmentShaderPath;
        
        VkDevice device;
        VkExtent2D swapChainExtent;
        VkFormat swapChainImageFormat;
        VkSampleCountFlagBits msaaSamples;

        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline graphicsPipeline = VK_NULL_HANDLE;

        // Helper methods (now public for direct access)
        void createRenderPass();
        void createDescriptorSetLayout();
        void createGraphicsPipeline(const char* vertexShaderPath, const char* fragmentShaderPath);
        void cleanup();
    };
}
