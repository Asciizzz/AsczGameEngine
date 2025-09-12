// RenderPass.cpp
#include "AzVulk/RenderPass.hpp"
#include <stdexcept>
#include <vector>

using namespace AzVulk;

// AttachmentConfig helper methods
AttachmentConfig AttachmentConfig::createColorAttachment(VkFormat format, VkImageLayout finalLayout) {
    AttachmentConfig config;
    config.format = format;
    config.samples = VK_SAMPLE_COUNT_1_BIT;
    config.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    config.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    config.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    config.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    config.finalLayout = finalLayout;
    return config;
}

AttachmentConfig AttachmentConfig::createDepthAttachment(VkFormat format, VkImageLayout finalLayout) {
    AttachmentConfig config;
    config.format = format;
    config.samples = VK_SAMPLE_COUNT_1_BIT;
    config.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    config.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    config.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    config.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    config.finalLayout = finalLayout;
    return config;
}

AttachmentConfig AttachmentConfig::createResolveAttachment(VkFormat format, VkImageLayout finalLayout) {
    AttachmentConfig config;
    config.format = format;
    config.samples = VK_SAMPLE_COUNT_1_BIT;
    config.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    config.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    config.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    config.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    config.finalLayout = finalLayout;
    return config;
}

// SubpassConfig helper methods
SubpassConfig SubpassConfig::createSimpleSubpass(uint32_t colorAttachment, uint32_t depthAttachment) {
    SubpassConfig config;
    if (colorAttachment != VK_ATTACHMENT_UNUSED) {
        config.colorAttachmentIndices.push_back(colorAttachment);
    }
    config.depthAttachmentIndex = depthAttachment;
    return config;
}

SubpassConfig SubpassConfig::createMRTSubpass(const std::vector<uint32_t>& colorAttachments, uint32_t depthAttachment) {
    SubpassConfig config;
    config.colorAttachmentIndices = colorAttachments;
    config.depthAttachmentIndex = depthAttachment;
    return config;
}

// RenderPassConfig factory methods
RenderPassConfig RenderPassConfig::createForwardRenderingConfig(VkFormat swapChainFormat) {
    RenderPassConfig config;
    
    // Color attachment (swapchain image)
    config.attachments.push_back(AttachmentConfig::createColorAttachment(swapChainFormat, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));
    
    // Depth attachment
    config.attachments.push_back(AttachmentConfig::createDepthAttachment());
    
    // Single subpass
    config.subpasses.push_back(SubpassConfig::createSimpleSubpass(0, 1));
    
    // Default dependency
    config.addDefaultDependency();
    
    return config;
}

RenderPassConfig RenderPassConfig::createDeferredGBufferConfig() {
    RenderPassConfig config;
    
    // Multiple render targets for G-buffer
    config.attachments.push_back(AttachmentConfig::createColorAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)); // Albedo
    config.attachments.push_back(AttachmentConfig::createColorAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)); // Normal
    config.attachments.push_back(AttachmentConfig::createColorAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)); // Material
    config.attachments.push_back(AttachmentConfig::createDepthAttachment(VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL));
    
    // MRT subpass
    config.subpasses.push_back(SubpassConfig::createMRTSubpass({0, 1, 2}, 3));
    
    config.addDefaultDependency();
    
    return config;
}

RenderPassConfig RenderPassConfig::createShadowMapConfig() {
    RenderPassConfig config;
    
    // Depth-only attachment
    config.attachments.push_back(AttachmentConfig::createDepthAttachment(VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL));
    
    // Depth-only subpass
    config.subpasses.push_back(SubpassConfig::createSimpleSubpass(VK_ATTACHMENT_UNUSED, 0));
    
    config.addDefaultDependency();
    
    return config;
}

RenderPassConfig RenderPassConfig::createPostProcessConfig(VkFormat colorFormat) {
    RenderPassConfig config;
    
    // Single color attachment for post-processing
    config.attachments.push_back(AttachmentConfig::createColorAttachment(colorFormat, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    
    // Simple subpass
    config.subpasses.push_back(SubpassConfig::createSimpleSubpass(0, VK_ATTACHMENT_UNUSED));
    
    config.addDefaultDependency();
    
    return config;
}

RenderPassConfig RenderPassConfig::createMSAAConfig(VkFormat colorFormat, VkFormat resolveFormat, VkSampleCountFlagBits samples) {
    RenderPassConfig config;
    
    // MSAA color attachment
    AttachmentConfig msaaColor = AttachmentConfig::createColorAttachment(colorFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    msaaColor.samples = samples;
    config.attachments.push_back(msaaColor);
    
    // Resolve attachment
    config.attachments.push_back(AttachmentConfig::createResolveAttachment(resolveFormat, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));
    
    // Depth attachment
    AttachmentConfig msaaDepth = AttachmentConfig::createDepthAttachment();
    msaaDepth.samples = samples;
    config.attachments.push_back(msaaDepth);
    
    // Subpass with resolve
    SubpassConfig subpass = SubpassConfig::createSimpleSubpass(0, 2);
    subpass.resolveAttachmentIndices.push_back(1);
    config.subpasses.push_back(subpass);
    
    config.addDefaultDependency();
    
    return config;
}

// RenderPassConfig helper methods
void RenderPassConfig::addDefaultDependency() {
    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dep.dependencyFlags = 0;
    dependencies.push_back(dep);
}

void RenderPassConfig::addDependency(uint32_t srcSubpass, uint32_t dstSubpass, 
                                    VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                                    VkAccessFlags srcAccess, VkAccessFlags dstAccess) {
    VkSubpassDependency dep{};
    dep.srcSubpass = srcSubpass;
    dep.dstSubpass = dstSubpass;
    dep.srcStageMask = srcStage;
    dep.dstStageMask = dstStage;
    dep.srcAccessMask = srcAccess;
    dep.dstAccessMask = dstAccess;
    dep.dependencyFlags = 0;
    dependencies.push_back(dep);
}

// RenderPass class implementation
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

void RenderPass::createRenderPass() {
    if (config.attachments.empty() || config.subpasses.empty()) {
        throw std::runtime_error("RenderPass must have at least one attachment and one subpass!");
    }

    // Create attachment descriptions
    auto attachmentDescs = createAttachmentDescriptions();

    // Create subpass descriptions
    std::vector<std::vector<VkAttachmentReference>> colorRefs(config.subpasses.size());
    std::vector<std::vector<VkAttachmentReference>> inputRefs(config.subpasses.size());
    std::vector<std::vector<VkAttachmentReference>> resolveRefs(config.subpasses.size());
    std::vector<VkAttachmentReference> depthRefs(config.subpasses.size());
    
    auto subpassDescs = createSubpassDescriptions(colorRefs, inputRefs, resolveRefs, depthRefs);

    // Create render pass
    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
    createInfo.pAttachments = attachmentDescs.data();
    createInfo.subpassCount = static_cast<uint32_t>(subpassDescs.size());
    createInfo.pSubpasses = subpassDescs.data();
    createInfo.dependencyCount = static_cast<uint32_t>(config.dependencies.size());
    createInfo.pDependencies = config.dependencies.empty() ? nullptr : config.dependencies.data();

    if (vkCreateRenderPass(lDevice, &createInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }
}

void RenderPass::cleanup() {
    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(lDevice, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }
}

std::vector<VkAttachmentDescription> RenderPass::createAttachmentDescriptions() const {
    std::vector<VkAttachmentDescription> descriptions;
    descriptions.reserve(config.attachments.size());

    for (const auto& attachment : config.attachments) {
        VkAttachmentDescription desc{};
        desc.flags = 0;
        desc.format = attachment.format;
        desc.samples = attachment.samples;
        desc.loadOp = attachment.loadOp;
        desc.storeOp = attachment.storeOp;
        desc.stencilLoadOp = attachment.stencilLoadOp;
        desc.stencilStoreOp = attachment.stencilStoreOp;
        desc.initialLayout = attachment.initialLayout;
        desc.finalLayout = attachment.finalLayout;
        descriptions.push_back(desc);
    }

    return descriptions;
}

std::vector<VkSubpassDescription> RenderPass::createSubpassDescriptions(
    std::vector<std::vector<VkAttachmentReference>>& colorRefs,
    std::vector<std::vector<VkAttachmentReference>>& inputRefs,
    std::vector<std::vector<VkAttachmentReference>>& resolveRefs,
    std::vector<VkAttachmentReference>& depthRefs) const {
    
    std::vector<VkSubpassDescription> descriptions;
    descriptions.reserve(config.subpasses.size());

    for (size_t i = 0; i < config.subpasses.size(); ++i) {
        const auto& subpass = config.subpasses[i];
        
        // Color attachments
        for (uint32_t colorIdx : subpass.colorAttachmentIndices) {
            if (colorIdx >= config.attachments.size()) {
                throw std::runtime_error("Color attachment index out of bounds!");
            }
            VkAttachmentReference ref{};
            ref.attachment = colorIdx;
            ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorRefs[i].push_back(ref);
        }

        // Input attachments
        for (uint32_t inputIdx : subpass.inputAttachmentIndices) {
            if (inputIdx >= config.attachments.size()) {
                throw std::runtime_error("Input attachment index out of bounds!");
            }
            VkAttachmentReference ref{};
            ref.attachment = inputIdx;
            ref.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            inputRefs[i].push_back(ref);
        }

        // Resolve attachments
        for (uint32_t resolveIdx : subpass.resolveAttachmentIndices) {
            if (resolveIdx >= config.attachments.size()) {
                throw std::runtime_error("Resolve attachment index out of bounds!");
            }
            VkAttachmentReference ref{};
            ref.attachment = resolveIdx;
            ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            resolveRefs[i].push_back(ref);
        }

        // Depth attachment
        if (subpass.depthAttachmentIndex != VK_ATTACHMENT_UNUSED) {
            if (subpass.depthAttachmentIndex >= config.attachments.size()) {
                throw std::runtime_error("Depth attachment index out of bounds!");
            }
            depthRefs[i].attachment = subpass.depthAttachmentIndex;
            depthRefs[i].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        // Validate resolve attachments count
        if (!resolveRefs[i].empty() && resolveRefs[i].size() != colorRefs[i].size()) {
            throw std::runtime_error("Resolve attachment count must match color attachment count!");
        }

        // Create subpass description
        VkSubpassDescription desc{};
        desc.flags = 0;
        desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        desc.inputAttachmentCount = static_cast<uint32_t>(inputRefs[i].size());
        desc.pInputAttachments = inputRefs[i].empty() ? nullptr : inputRefs[i].data();
        desc.colorAttachmentCount = static_cast<uint32_t>(colorRefs[i].size());
        desc.pColorAttachments = colorRefs[i].empty() ? nullptr : colorRefs[i].data();
        desc.pResolveAttachments = resolveRefs[i].empty() ? nullptr : resolveRefs[i].data();
        desc.pDepthStencilAttachment = (subpass.depthAttachmentIndex != VK_ATTACHMENT_UNUSED) ? &depthRefs[i] : nullptr;
        desc.preserveAttachmentCount = 0;
        desc.pPreserveAttachments = nullptr;

        descriptions.push_back(desc);
    }

    return descriptions;
}