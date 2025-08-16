#pragma once

#include <vulkan/vulkan.h>
#include <fstream>

#include "Helpers/Templates.hpp"

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

        // Renderpass and descriptor setLayouts
        VkRenderPass renderPass = VK_NULL_HANDLE;
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

        // Static factory methods for common configurations
        static RasterPipelineConfig createOpaqueConfig(VkSampleCountFlagBits msaaSamples, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts);
        static RasterPipelineConfig createTransparentConfig(VkSampleCountFlagBits msaaSamples, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts);
        static RasterPipelineConfig createSkyConfig(VkSampleCountFlagBits msaaSamples, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts);
    };

    class Pipeline {
    public:
        Pipeline(VkDevice device, const RasterPipelineConfig& config,
                const char* vertexShaderPath, const char* fragmentShaderPath);
        ~Pipeline() { cleanup(); } void cleanup();
        
        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;


        RasterPipelineConfig config;

        const char* vertexShaderPath;
        const char* fragmentShaderPath;

        VkDevice device;

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline graphicsPipeline = VK_NULL_HANDLE;

        void createGraphicPipeline();
        void recreateGraphicPipeline(VkRenderPass newRenderPass);


        // Helper functions
        static std::vector<char> readShaderFile(const std::string& filename);
    };
}
