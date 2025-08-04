#pragma once

#include <vulkan/vulkan.h>
#include "AzVulk/Buffer.hpp"

namespace AzVulk {
    class RasterPipeline {
    public:
        RasterPipeline( VkDevice device, VkExtent2D swapChainExtent, VkFormat swapChainImageFormat,
                        const char* vertexShaderPath, const char* fragmentShaderPath,
                        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT);
        ~RasterPipeline();        
        
        RasterPipeline(const RasterPipeline&) = delete;
        RasterPipeline& operator=(const RasterPipeline&) = delete;

        void recreate(VkExtent2D newExtent, VkFormat newFormat, VkFormat depthFormat, VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT);

        // Shader paths
        const char* vertexShaderPath;
        const char* fragmentShaderPath;
        
        // Device and format context
        VkDevice device;
        VkExtent2D swapChainExtent;
        VkFormat swapChainImageFormat;
        VkSampleCountFlagBits msaaSamples; // anti-aliasing samples

        VkRenderPass renderPass = VK_NULL_HANDLE;

        // Pipeline layout and objects
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline graphicsPipeline = VK_NULL_HANDLE;
        VkPipeline transparentPipeline = VK_NULL_HANDLE; // Alpha-blended pipeline for transparent objects

        void createRenderPass();
        void createDescriptorSetLayout();
        void createGraphicsPipeline();
        void createTransparentPipeline(); // Create alpha-blended version
        void cleanup();
    };
}
