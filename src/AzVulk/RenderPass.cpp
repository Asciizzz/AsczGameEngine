// RenderPass.cpp
#include "AzVulk/RenderPass.hpp"
#include <stdexcept>
#include <vector>

namespace AzVulk {

RenderPassConfig RenderPassConfig::createForwardRenderingConfig(VkFormat swapChainFormat, VkSampleCountFlagBits msaaSamples) {
    RenderPassConfig config;
    config.colorFormat = swapChainFormat;
    config.colorSamples = msaaSamples;
    config.depthSamples = msaaSamples;
    config.hasDepth = true;

    if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
        config.hasResolve = true;
        config.resolveFormat = swapChainFormat;
    }
    return config;
}

RenderPassConfig RenderPassConfig::createDeferredGBufferConfig(VkSampleCountFlagBits msaaSamples) {
    RenderPassConfig config;
    config.colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    config.colorSamples = msaaSamples;
    config.hasDepth = true;
    config.depthSamples = msaaSamples;
    config.hasResolve = false;
    return config;
}

RenderPassConfig RenderPassConfig::createShadowMapConfig() {
    RenderPassConfig config;
    config.colorFormat = VK_FORMAT_UNDEFINED;
    config.hasDepth = true;
    config.depthFormat = VK_FORMAT_D32_SFLOAT;
    config.depthSamples = VK_SAMPLE_COUNT_1_BIT;
    config.hasResolve = false;
    return config;
}

RenderPass::RenderPass(VkDevice lDevice, VkPhysicalDevice pDevice, const RenderPassConfig& config)
    : lDevice(lDevice), pDevice(pDevice), config(config) {
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


// ---------- safer chooseDepthResolveMode ----------
VkResolveModeFlagBits RenderPass::chooseDepthResolveMode(VkPhysicalDevice pDevice) {
    // This function now returns VK_RESOLVE_MODE_NONE, as depth resolve properties require Vulkan 1.2+ and VkPhysicalDeviceProperties2.
    // If you want to support depth resolve, you must use VkPhysicalDeviceProperties2 and VkPhysicalDeviceDepthStencilResolveProperties.
    // For compatibility, we revert to NONE here.
    return VK_RESOLVE_MODE_NONE;
}

// ---------- safer createRenderPass ----------
void RenderPass::createRenderPass() {
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> colorRefs;
    VkAttachmentReference depthRef{};
    VkAttachmentReference colorResolveRef{};
    VkAttachmentReference depthResolveRef{};

    uint32_t attachmentIndex = 0;

    // --- Depth attachment
    if (config.hasDepth) {
        VkAttachmentDescription depth{};
        depth.flags = 0;
        depth.format = config.depthFormat;
        depth.samples = config.depthSamples;
        depth.loadOp = config.depthLoadOp;
        depth.storeOp = config.depthStoreOp;
        depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments.push_back(depth);

        depthRef.attachment = attachmentIndex++;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    // --- Color attachment
    bool hasResolve = config.hasResolve;
    if (config.colorFormat != VK_FORMAT_UNDEFINED) {
        VkAttachmentDescription color{};
        color.flags = 0;
        color.format = config.colorFormat;
        color.samples = config.colorSamples;
        color.loadOp = config.colorLoadOp;
        color.storeOp = config.colorStoreOp;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = hasResolve ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments.push_back(color);

        VkAttachmentReference ref{};
        ref.attachment = attachmentIndex++;
        ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorRefs.push_back(ref);
    }

    // --- Resolve attachment (for MSAA)
    if (hasResolve) {
        VkAttachmentDescription resolve{};
        resolve.flags = 0;
        resolve.format = config.resolveFormat;
        resolve.samples = VK_SAMPLE_COUNT_1_BIT;
        resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments.push_back(resolve);

        colorResolveRef.attachment = attachmentIndex++;
        colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    // --- Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
    subpass.pColorAttachments = colorRefs.empty() ? nullptr : colorRefs.data();
    subpass.pDepthStencilAttachment = config.hasDepth ? &depthRef : nullptr;
    subpass.pResolveAttachments = hasResolve ? &colorResolveRef : nullptr;

    // --- Dependency
    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dep.dependencyFlags = 0;

    // --- RenderPass create
    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.attachmentCount = static_cast<uint32_t>(attachments.size());
    info.pAttachments = attachments.data();
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dep;

    if (vkCreateRenderPass(lDevice, &info, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }
}


void RenderPass::cleanup() {
    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(lDevice, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }
}

} // namespace AzVulk
