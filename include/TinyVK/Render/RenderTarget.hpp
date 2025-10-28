#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace tinyVK {

// Forward declarations
class ImageVK;

/**
 * @brief Represents a single render attachment with Vulkan resources and clear value
 */
struct RenderAttachment {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkClearValue clearValue{};
    
    // Constructors - moved to implementation for forward declaration support
    RenderAttachment();
    RenderAttachment(VkImage img, VkImageView v);
    RenderAttachment(VkImage img, VkImageView v, VkClearValue clear);
    RenderAttachment(const ImageVK& imageVK, VkClearValue clear = {});
    
    // Copy and move operations
    RenderAttachment(const RenderAttachment&) = default;
    RenderAttachment& operator=(const RenderAttachment&) = default;
    RenderAttachment(RenderAttachment&&) = default;
    RenderAttachment& operator=(RenderAttachment&&) = default;
};

/**
 * @brief Lightweight wrapper that references existing Vulkan resources for rendering
 * 
 * This is a non-owning wrapper that contains references to:
 * - VkImage(s): The actual image resources (color/depth/etc.)
 * - VkImageView(s): Views into those images
 * - VkFramebuffer: Framebuffer containing the image views
 * - VkRenderPass: Compatible render pass for this target
 * 
 * Benefits:
 * - Minimal memory footprint
 * - Dynamic attachment support
 * - No resource ownership/management overhead
 * - Easy switching between render targets
 */
class RenderTarget {
public:
    RenderTarget() = default;
    ~RenderTarget() = default;

    RenderTarget(VkRenderPass rp, VkFramebuffer fb, VkExtent2D ext) 
        : renderPass(rp), framebuffer(fb), extent(ext) {}

    // Copyable and movable since we're just storing handles
    RenderTarget(const RenderTarget&) = default;
    RenderTarget& operator=(const RenderTarget&) = default;
    RenderTarget(RenderTarget&&) = default;
    RenderTarget& operator=(RenderTarget&&) = default;

    // Setup/modification
    RenderTarget& withRenderPass(VkRenderPass rp);
    RenderTarget& withFrameBuffer(VkFramebuffer fb);
    RenderTarget& withExtent(VkExtent2D ext);

    RenderTarget& addAttachment(const RenderAttachment& attachment);
    RenderTarget& addAttachment(VkImage image, VkImageView view, VkClearValue clearValue = {});
    RenderTarget& clearAttachments();

    // Core rendering interface
    void beginRenderPass(VkCommandBuffer cmd, VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE) const;
    void endRenderPass(VkCommandBuffer cmd) const;
    
    // Viewport/scissor setup
    void setViewportAndScissor(VkCommandBuffer cmd) const;
    void setViewport(VkCommandBuffer cmd, const VkViewport& viewport) const;
    void setScissor(VkCommandBuffer cmd, const VkRect2D& scissor) const;

    // Resource access
    VkRenderPass getRenderPass() const { return renderPass; }
    VkFramebuffer getFramebuffer() const { return framebuffer; }
    VkExtent2D getExtent() const { return extent; }
    
    // Attachment access
    size_t getAttachmentCount() const { return attachments.size(); }
    const RenderAttachment& getAttachment(size_t index) const { return attachments[index]; }
    const std::vector<RenderAttachment>& getAttachments() const { return attachments; }

    // State queries
    bool valid() const { return renderPass != VK_NULL_HANDLE && framebuffer != VK_NULL_HANDLE; }
    bool hasAttachments() const { return !attachments.empty(); }

private:
    // Core Vulkan resources (non-owning references)
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkExtent2D extent{};

    // Dynamic attachments
    std::vector<RenderAttachment> attachments;
};



} // namespace tinyVK