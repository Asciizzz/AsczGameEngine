#include "tinyVk/Render/RenderPass.hpp"

#include <stdexcept>

using namespace tinyVk;

RenderPass::RenderPass(VkDevice device, const RenderPassConfig& config)
: device(device) {
    std::vector<VkAttachmentDescription> attachments;
    attachments.reserve(config.attachments.size());

    for (auto& a : config.attachments) {
        VkAttachmentDescription desc{};
        desc.format         = a.format;
        desc.samples        = a.samples;
        desc.loadOp         = a.loadOp;
        desc.storeOp        = a.storeOp;
        desc.stencilLoadOp  = a.stencilLoadOp;
        desc.stencilStoreOp = a.stencilStoreOp;
        desc.initialLayout  = a.initialLayout;
        desc.finalLayout    = a.finalLayout;
        attachments.push_back(desc);
    }

    std::vector<VkSubpassDescription> subpasses;
    std::vector<std::vector<VkAttachmentReference>> colorRefs(config.subpasses.size());
    std::vector<VkAttachmentReference> depthRefs(config.subpasses.size());

    for (size_t i = 0; i < config.subpasses.size(); i++) {
        const auto& sp = config.subpasses[i];

        for (auto indx : sp.colorAttachments) {
            VkAttachmentReference ref{};
            ref.attachment = indx;
            ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            colorRefs[i].push_back(ref);
        }

        VkAttachmentReference* depthRefPtr = nullptr;
        if (sp.depthAttachment >= 0) {
            VkAttachmentReference ref{};
            ref.attachment = (uint32_t)sp.depthAttachment;
            ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            depthRefs[i] = ref;
            depthRefPtr = &depthRefs[i];
        }

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs[i].size());
        subpass.pColorAttachments = colorRefs[i].data();
        subpass.pDepthStencilAttachment = depthRefPtr;
        subpasses.push_back(subpass);
    }

    VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    info.attachmentCount = (uint32_t)attachments.size();
    info.pAttachments = attachments.data();
    info.subpassCount = (uint32_t)subpasses.size();
    info.pSubpasses = subpasses.data();
    info.dependencyCount = (uint32_t)config.dependencies.size();
    info.pDependencies = config.dependencies.data();

    if (vkCreateRenderPass(device, &info, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create RenderPass");
    }
}

RenderPass::~RenderPass() {
    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass, nullptr);
    }
}

// AttachmentConfig fluent interface implementations
AttachmentConfig& AttachmentConfig::withFormat(VkFormat fmt) {
    format = fmt;
    return *this;
}

AttachmentConfig& AttachmentConfig::withInitialLayout(VkImageLayout layout) {
    initialLayout = layout;
    return *this;
}

AttachmentConfig& AttachmentConfig::withFinalLayout(VkImageLayout layout) {
    finalLayout = layout;
    return *this;
}

AttachmentConfig& AttachmentConfig::withLoadOp(VkAttachmentLoadOp op) {
    loadOp = op;
    return *this;
}

AttachmentConfig& AttachmentConfig::withStoreOp(VkAttachmentStoreOp op) {
    storeOp = op;
    return *this;
}

AttachmentConfig& AttachmentConfig::withStencilLoadOp(VkAttachmentLoadOp op) {
    stencilLoadOp = op;
    return *this;
}

AttachmentConfig& AttachmentConfig::withStencilStoreOp(VkAttachmentStoreOp op) {
    stencilStoreOp = op;
    return *this;
}

AttachmentConfig& AttachmentConfig::withSamples(VkSampleCountFlagBits sampleCount) {
    samples = sampleCount;
    return *this;
}

AttachmentConfig& AttachmentConfig::asColorAttachment() {
    finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    return *this;
}

AttachmentConfig& AttachmentConfig::asDepthAttachment() {
    finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    return *this;
}

AttachmentConfig& AttachmentConfig::asShaderReadOnly() {
    finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    return *this;
}

AttachmentConfig& AttachmentConfig::asGeneral() {
    finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    return *this;
}

AttachmentConfig& AttachmentConfig::asPresent() {
    finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    return *this;
}

AttachmentConfig& AttachmentConfig::preserveContent() {
    loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    return *this;
}

AttachmentConfig& AttachmentConfig::clearContent() {
    loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    return *this;
}

AttachmentConfig& AttachmentConfig::dontCare() {
    loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    return *this;
}

// SubpassConfig fluent interface implementations
SubpassConfig& SubpassConfig::withColorAttachment(uint32_t index) {
    colorAttachments.push_back(index);
    return *this;
}

SubpassConfig& SubpassConfig::withColorAttachments(const std::vector<uint32_t>& indices) {
    colorAttachments = indices;
    return *this;
}

SubpassConfig& SubpassConfig::withDepthAttachment(uint32_t index) {
    depthAttachment = static_cast<int32_t>(index);
    return *this;
}

SubpassConfig& SubpassConfig::withoutDepthAttachment() {
    depthAttachment = -1;
    return *this;
}

SubpassConfig SubpassConfig::simple(uint32_t colorIndex, int32_t depthIndex) {
    SubpassConfig config;
    config.colorAttachments = {colorIndex};
    config.depthAttachment = depthIndex;
    return config;
}

SubpassConfig SubpassConfig::multipleRenderTargets(const std::vector<uint32_t>& colorIndices, int32_t depthIndex) {
    SubpassConfig config;
    config.colorAttachments = colorIndices;
    config.depthAttachment = depthIndex;
    return config;
}

// RenderPassConfig fluent interface implementations
RenderPassConfig& RenderPassConfig::addAttachment(const AttachmentConfig& attachment) {
    attachments.push_back(attachment);
    return *this;
}

RenderPassConfig& RenderPassConfig::withSubpass(const SubpassConfig& subpass) {
    subpasses.push_back(subpass);
    return *this;
}

RenderPassConfig& RenderPassConfig::withDependency(const VkSubpassDependency& dependency) {
    dependencies.push_back(dependency);
    return *this;
}

RenderPassConfig& RenderPassConfig::withStandardDependency() {
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies.push_back(dependency);
    return *this;
}

RenderPassConfig& RenderPassConfig::withImGuiDependency() {
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies.push_back(dependency);
    return *this;
}

RenderPassConfig RenderPassConfig::forwardRendering(VkFormat colorFormat, VkFormat depthFormat) {
    return RenderPassConfig()
        .addAttachment(AttachmentConfig().withFormat(colorFormat).asPresent())
        .addAttachment(AttachmentConfig().withFormat(depthFormat).asDepthAttachment())
        .withSubpass(SubpassConfig::simple(0, 1))
        .withStandardDependency();
}

RenderPassConfig RenderPassConfig::offscreenRendering(VkFormat colorFormat, VkFormat depthFormat) {
    return RenderPassConfig()
        .addAttachment(AttachmentConfig().withFormat(colorFormat).asGeneral())
        .addAttachment(AttachmentConfig().withFormat(depthFormat).asDepthAttachment())
        .withSubpass(SubpassConfig::simple(0, 1))
        .withStandardDependency();
}

RenderPassConfig RenderPassConfig::imguiOverlay(VkFormat colorFormat, VkFormat depthFormat) {
    return RenderPassConfig()
        .addAttachment(AttachmentConfig().withFormat(colorFormat).preserveContent().asPresent())
        .addAttachment(AttachmentConfig().withFormat(depthFormat).dontCare().asDepthAttachment())
        .withSubpass(SubpassConfig::simple(0, 1))
        .withStandardDependency();
}
