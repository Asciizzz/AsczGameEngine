#pragma once

#include <vulkan/vulkan.h>
#include "AzVulk/Buffer.hpp"

namespace AzVulk {
    struct RasterPipelineConfig {
        // Rasterization settings
        VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
        VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
        VkBool32 depthBiasEnable = VK_FALSE;
        
        // Depth/Stencil settings
        VkBool32 depthTestEnable = VK_TRUE;
        VkBool32 depthWriteEnable = VK_TRUE;
        VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
        
        // Color blending settings
        VkBool32 blendEnable = VK_FALSE;
        VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        VkBlendOp colorBlendOp = VK_BLEND_OP_ADD;
        VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD;
        
        // Multisampling settings
        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        VkBool32 sampleShadingEnable = VK_TRUE;
        float minSampleShading = 0.2f;
        
        // Static factory methods for common configurations
        static RasterPipelineConfig createOpaqueConfig(VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_4_BIT);
        static RasterPipelineConfig createTransparentConfig(VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_4_BIT);
        static RasterPipelineConfig createSkyConfig(VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_4_BIT);
    };

    class Pipeline {
    public:
        Pipeline( VkDevice device, VkRenderPass renderPass,
                        std::vector<VkDescriptorSetLayout> descriptorSetLayouts,
                        const char* vertexShaderPath, const char* fragmentShaderPath,
                        const RasterPipelineConfig& config);
        ~Pipeline();
        
        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;

        void recreate(VkRenderPass newRenderPass, const RasterPipelineConfig& newConfig);

        // Configuration
        RasterPipelineConfig config;
        
        // Shader paths
        const char* vertexShaderPath;
        const char* fragmentShaderPath;
        
        // References to Vulkan objects
        VkDevice device;
        VkRenderPass renderPass;

        // Pipeline layout and objects
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline graphicsPipeline = VK_NULL_HANDLE;

        void createGraphicsPipeline();
        void cleanup();
    };
}
