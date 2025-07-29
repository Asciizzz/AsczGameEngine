#pragma once

#include <vulkan/vulkan.h>
#include "AzVulk/Buffer.hpp"

namespace AzVulk {
    class RasterPipeline {
    public:
        RasterPipeline(VkDevice device, VkExtent2D swapChainExtent, VkFormat swapChainImageFormat,
                      const char* vertexShaderPath, const char* fragmentShaderPath,
                      const char* billboardVertexShaderPath, const char* billboardFragmentShaderPath,
                      VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT);
        ~RasterPipeline();        // Delete copy constructor and assignment operator
        RasterPipeline(const RasterPipeline&) = delete;
        RasterPipeline& operator=(const RasterPipeline&) = delete;

        void recreate(VkExtent2D newExtent, VkFormat newFormat, VkFormat depthFormat, VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT);

        // Remove the separate createBillboardPipeline method since it's now integrated

        
        const char* vertexShaderPath;
        const char* fragmentShaderPath;
        const char* billboardVertexShaderPath;
        const char* billboardFragmentShaderPath;
        
        VkDevice device;
        VkExtent2D swapChainExtent;
        VkFormat swapChainImageFormat;
        VkSampleCountFlagBits msaaSamples;

        VkRenderPass renderPass = VK_NULL_HANDLE;

        // Descriptor set layout for the main pipeline
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline graphicsPipeline = VK_NULL_HANDLE;
        
        // Billboard support within the same render pass
        VkDescriptorSetLayout billboardDescriptorSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout billboardPipelineLayout = VK_NULL_HANDLE;
        VkPipeline billboardGraphicsPipeline = VK_NULL_HANDLE;

        // Helper methods (now public for direct access)
        void createRenderPass();
        void createDescriptorSetLayout();
        void createGraphicsPipeline(const char* vertexShaderPath, const char* fragmentShaderPath);
        void createBillboardPipeline(const char* vertexShaderPath, const char* fragmentShaderPath);
        void cleanup();
    };
}
