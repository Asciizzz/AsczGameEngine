#include "AzVulk/RenderPass.hpp"
#include <stdexcept>
#include <array>
#include <vector>

namespace AzVulk {

    RenderPassConfig RenderPassConfig::createForwardRenderingConfig(VkFormat swapChainFormat, VkSampleCountFlagBits msaaSamples) {
        RenderPassConfig config;
        config.colorFormat = swapChainFormat;
        config.colorSamples = msaaSamples;
        config.hasDepth = true;
        config.depthSamples = msaaSamples;
        
        if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
            config.hasResolve = true;
            config.resolveFormat = swapChainFormat;
        }
        
        return config;
    }

    RenderPassConfig RenderPassConfig::createDeferredGBufferConfig(VkSampleCountFlagBits msaaSamples) {
        RenderPassConfig config;
        config.colorFormat = VK_FORMAT_R8G8B8A8_UNORM; // Albedo
        config.colorSamples = msaaSamples;
        config.hasDepth = true;
        config.depthSamples = msaaSamples;
        config.hasResolve = false; // G-buffer typically doesn't need resolve
        
        return config;
    }

    RenderPassConfig RenderPassConfig::createShadowMapConfig() {
        RenderPassConfig config;
        config.colorFormat = VK_FORMAT_UNDEFINED; // No color attachment
        config.hasDepth = true;
        config.depthFormat = VK_FORMAT_D32_SFLOAT;
        config.depthSamples = VK_SAMPLE_COUNT_1_BIT;
        config.hasResolve = false;
        
        return config;
    }

    RenderPass::RenderPass(VkDevice device, const RenderPassConfig& config)
        : device(device), config(config) {
        createRenderPass();
    }

    RenderPass::~RenderPass() {
        cleanup();
    }

    void RenderPass::recreate(const RenderPassConfig& newConfig) {
        cleanup();
        this->config = newConfig;
        createRenderPass();
    }

    void RenderPass::createRenderPass() {
        std::vector<VkAttachmentDescription> attachments;
        std::vector<VkAttachmentReference> colorAttachmentRefs;
        VkAttachmentReference depthAttachmentRef{};
        VkAttachmentReference resolveAttachmentRef{};
        
        uint32_t attachmentIndex = 0;
        
        // Color attachment (if present)
        if (config.colorFormat != VK_FORMAT_UNDEFINED) {
            VkAttachmentDescription colorAttachment{};
            colorAttachment.format = config.colorFormat;
            colorAttachment.samples = config.colorSamples;
            colorAttachment.loadOp = config.colorLoadOp;
            colorAttachment.storeOp = config.colorStoreOp;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = config.hasResolve ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            
            attachments.push_back(colorAttachment);
            
            VkAttachmentReference colorAttachmentRef{};
            colorAttachmentRef.attachment = attachmentIndex++;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachmentRefs.push_back(colorAttachmentRef);
        }
        
        // Depth attachment (if present)
        if (config.hasDepth) {
            VkAttachmentDescription depthAttachment{};
            depthAttachment.format = config.depthFormat;
            depthAttachment.samples = config.depthSamples;
            depthAttachment.loadOp = config.depthLoadOp;
            depthAttachment.storeOp = config.depthStoreOp;
            depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            
            attachments.push_back(depthAttachment);
            
            depthAttachmentRef.attachment = attachmentIndex++;
            depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
        
        // Resolve attachment (if MSAA is enabled)
        if (config.hasResolve) {
            VkAttachmentDescription resolveAttachment{};
            resolveAttachment.format = config.resolveFormat;
            resolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            resolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            resolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            resolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            resolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            resolveAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            
            attachments.push_back(resolveAttachment);
            
            resolveAttachmentRef.attachment = attachmentIndex++;
            resolveAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        
        // Create subpass
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size());
        subpass.pColorAttachments = colorAttachmentRefs.empty() ? nullptr : colorAttachmentRefs.data();
        subpass.pDepthStencilAttachment = config.hasDepth ? &depthAttachmentRef : nullptr;
        subpass.pResolveAttachments = config.hasResolve ? &resolveAttachmentRef : nullptr;
        
        // Subpass dependencies
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        
        // Create render pass
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;
        
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass!");
        }
    }

    void RenderPass::cleanup() {
        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }
    }

} // namespace AzVulk
