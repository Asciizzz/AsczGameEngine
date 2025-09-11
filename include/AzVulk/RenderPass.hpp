#pragma once

#include <vulkan/vulkan.h>

namespace AzVulk {
    
    struct RenderPassConfig {
        // Color attachment settings
        VkFormat              colorFormat;
        VkSampleCountFlagBits colorSamples  = VK_SAMPLE_COUNT_1_BIT;
        VkAttachmentLoadOp    colorLoadOp   = VK_ATTACHMENT_LOAD_OP_CLEAR;
        VkAttachmentStoreOp   colorStoreOp  = VK_ATTACHMENT_STORE_OP_STORE;

        // Depth attachment settings
        bool                  hasDepth      = true;
        VkFormat              depthFormat   = VK_FORMAT_D32_SFLOAT;
        VkSampleCountFlagBits depthSamples  = VK_SAMPLE_COUNT_1_BIT;
        VkAttachmentLoadOp    depthLoadOp   = VK_ATTACHMENT_LOAD_OP_CLEAR;
        VkAttachmentStoreOp   depthStoreOp  = VK_ATTACHMENT_STORE_OP_STORE;  // Store for sampling

        // MSAA resolve settings
        bool                  hasResolve    = false;
        VkFormat              resolveFormat = VK_FORMAT_UNDEFINED;

        // Factory methods for common configurations
        static RenderPassConfig createForwardRenderingConfig(VkFormat swapChainFormat);
        static RenderPassConfig createDeferredGBufferConfig();
        static RenderPassConfig createShadowMapConfig();
    };

    class RenderPass {
    public:
        RenderPass(VkDevice lDevice, VkPhysicalDevice pDevice, const RenderPassConfig& config);
        ~RenderPass();
        
        RenderPass(const RenderPass&) = delete;
        RenderPass& operator=(const RenderPass&) = delete;
        
        void recreate(const RenderPassConfig& newConfig);

        VkDevice lDevice;
        VkPhysicalDevice pDevice;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        RenderPassConfig config;

        VkRenderPass get() const { return renderPass; }

        void createRenderPass();
        void cleanup();
    };
    
} // namespace AzVulk
