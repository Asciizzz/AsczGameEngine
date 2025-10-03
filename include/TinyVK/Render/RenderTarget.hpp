#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <functional>
#include <string>
#include <unordered_map>

namespace TinyVK {

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
    // Constructors - moved to implementation for forward declaration support
    RenderTarget();
    RenderTarget(VkRenderPass renderPass, VkFramebuffer framebuffer, VkExtent2D extent);
    RenderTarget(VkRenderPass renderPass, VkFramebuffer framebuffer, VkExtent2D extent,
                std::vector<RenderAttachment> attachments);

    // Default destructor - we don't own anything
    ~RenderTarget() = default;

    // Copyable and movable since we're just storing handles
    RenderTarget(const RenderTarget&) = default;
    RenderTarget& operator=(const RenderTarget&) = default;
    RenderTarget(RenderTarget&&) = default;
    RenderTarget& operator=(RenderTarget&&) = default;

    // Setup/modification
    void withRenderPass(VkRenderPass rp) { renderPass = rp; }
    void withFrameBuffer(VkFramebuffer fb) { framebuffer = fb; }
    void withExtent(VkExtent2D ext) { extent = ext; }
    
    void addAttachment(const RenderAttachment& attachment) { attachments.push_back(attachment); }
    void addAttachment(VkImage image, VkImageView view, VkClearValue clearValue = {}) {
        attachments.emplace_back(image, view, clearValue);
    }
    
    void clearAttachments() { attachments.clear(); }

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
    
    // Convenience accessors (assumes standard layout: color attachments first, then depth)
    VkImage getColorImage(size_t index = 0) const { 
        return (index < attachments.size()) ? attachments[index].image : VK_NULL_HANDLE; 
    }
    VkImageView getColorImageView(size_t index = 0) const { 
        return (index < attachments.size()) ? attachments[index].view : VK_NULL_HANDLE; 
    }
    
    // State queries
    bool isValid() const { return renderPass != VK_NULL_HANDLE && framebuffer != VK_NULL_HANDLE; }
    bool hasAttachments() const { return !attachments.empty(); }

    // Utility: Execute a render function with automatic begin/end
    template<typename RenderFunc>
    void render(VkCommandBuffer cmd, RenderFunc&& renderFunc) const {
        beginRenderPass(cmd);
        renderFunc(cmd, getRenderPass(), getFramebuffer());
        endRenderPass(cmd);
    }

private:
    // Core Vulkan resources (non-owning references)
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkExtent2D extent{};
    
    // Dynamic attachments
    std::vector<RenderAttachment> attachments;
};

/**
 * @brief Simple manager for multiple render targets with name-based access
 */
class RenderTargetManager {
public:
    RenderTargetManager() = default;
    ~RenderTargetManager() = default;

    // Add named render targets
    void addTarget(const std::string& name, const RenderTarget& target) {
        targets[name] = target;
    }
    
    void addTarget(const std::string& name, VkRenderPass renderPass, VkFramebuffer framebuffer, VkExtent2D extent) {
        targets[name] = RenderTarget(renderPass, framebuffer, extent);
    }
    
    // Access targets
    RenderTarget* getTarget(const std::string& name) {
        auto it = targets.find(name);
        return (it != targets.end()) ? &it->second : nullptr;
    }
    
    const RenderTarget* getTarget(const std::string& name) const {
        auto it = targets.find(name);
        return (it != targets.end()) ? &it->second : nullptr;
    }
    
    RenderTarget* getCurrentTarget() { return currentTarget; }
    
    // Switch active target
    void setActiveTarget(const std::string& name) {
        currentTarget = getTarget(name);
    }
    void setActiveTarget(RenderTarget* target) { currentTarget = target; }
    
    // Convenience rendering with automatic target management
    template<typename RenderFunc>
    void renderTo(const std::string& targetName, VkCommandBuffer cmd, RenderFunc&& renderFunc) {
        auto* target = getTarget(targetName);
        if (target && target->isValid()) {
            target->render(cmd, std::forward<RenderFunc>(renderFunc));
        }
    }

    // Management
    void removeTarget(const std::string& name) { targets.erase(name); }
    void clear() { targets.clear(); currentTarget = nullptr; }
    bool hasTarget(const std::string& name) const { return targets.find(name) != targets.end(); }

private:
    std::unordered_map<std::string, RenderTarget> targets;
    RenderTarget* currentTarget = nullptr;
};

} // namespace TinyVK