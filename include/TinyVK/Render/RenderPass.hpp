#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace TinyVK {

struct AttachmentConfig {
    VkFormat format;
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

    // Fluent builder interface
    AttachmentConfig& withFormat(VkFormat fmt);
    AttachmentConfig& withInitialLayout(VkImageLayout layout);
    AttachmentConfig& withFinalLayout(VkImageLayout layout);
    AttachmentConfig& withLoadOp(VkAttachmentLoadOp op);
    AttachmentConfig& withStoreOp(VkAttachmentStoreOp op);
    AttachmentConfig& withStencilLoadOp(VkAttachmentLoadOp op);
    AttachmentConfig& withStencilStoreOp(VkAttachmentStoreOp op);
    AttachmentConfig& withSamples(VkSampleCountFlagBits sampleCount);
    
    // Convenience methods for common configurations
    AttachmentConfig& asColorAttachment();
    AttachmentConfig& asDepthAttachment();
    AttachmentConfig& asShaderReadOnly();
    AttachmentConfig& asGeneral();
    AttachmentConfig& asPresent();
    AttachmentConfig& preserveContent();
    AttachmentConfig& clearContent();
    AttachmentConfig& dontCare();
};

struct SubpassConfig {
    std::vector<uint32_t> colorAttachments; // indices into attachments
    int32_t depthAttachment = -1;           // index into attachments, or -1

    // Fluent builder interface
    SubpassConfig& withColorAttachment(uint32_t index);
    SubpassConfig& withColorAttachments(const std::vector<uint32_t>& indices);
    SubpassConfig& withDepthAttachment(uint32_t index);
    SubpassConfig& withoutDepthAttachment();
    
    // Static factory methods
    static SubpassConfig simple(uint32_t colorIndex, int32_t depthIndex = -1);
    static SubpassConfig multipleRenderTargets(const std::vector<uint32_t>& colorIndices, int32_t depthIndex = -1);
};

struct RenderPassConfig {
    std::vector<AttachmentConfig> attachments;
    std::vector<SubpassConfig> subpasses;
    // (Optional) dependencies between subpasses
    std::vector<VkSubpassDependency> dependencies;

    // Fluent builder interface
    RenderPassConfig& withAttachment(const AttachmentConfig& attachment);
    RenderPassConfig& withSubpass(const SubpassConfig& subpass);
    RenderPassConfig& withDependency(const VkSubpassDependency& dependency);
    RenderPassConfig& withStandardDependency();
    RenderPassConfig& withImGuiDependency();
    
    // Static factory methods for common render pass types
    static RenderPassConfig forwardRendering(VkFormat colorFormat, VkFormat depthFormat);
    static RenderPassConfig offscreenRendering(VkFormat colorFormat, VkFormat depthFormat);
    static RenderPassConfig imguiOverlay(VkFormat colorFormat, VkFormat depthFormat);
};

class RenderPass {
public:
    RenderPass(VkDevice device, const RenderPassConfig& config);
    ~RenderPass();

    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    VkRenderPass get() const { return renderPass; }
    operator VkRenderPass() const { return renderPass; }

private:
    VkDevice device;
    VkRenderPass renderPass = VK_NULL_HANDLE;
};

} // namespace TinyVK
