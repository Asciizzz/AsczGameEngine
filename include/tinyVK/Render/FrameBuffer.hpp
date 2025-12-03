#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace tinyVk {

struct FrameBufferConfig {
    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::vector<VkImageView> attachments;
    VkExtent2D extent = {0, 0};
    uint32_t layers = 1;

    FrameBufferConfig& withRenderPass(VkRenderPass rp);
    FrameBufferConfig& addAttachment(VkImageView att);
    FrameBufferConfig& addAttachments(const std::vector<VkImageView>& atts);
    FrameBufferConfig& withExtent(VkExtent2D ext);
    FrameBufferConfig& withExtent(uint32_t width, uint32_t height);
    FrameBufferConfig& withLayers(uint32_t layers);
};

class FrameBuffer {
public:
    FrameBuffer(VkDevice device) : device(device) {}
    ~FrameBuffer();
    void cleanup();

    FrameBuffer(const FrameBuffer&) = delete;
    FrameBuffer& operator=(const FrameBuffer&) = delete;

    FrameBuffer(FrameBuffer&& other) noexcept;
    FrameBuffer& operator=(FrameBuffer&& other) noexcept;

    VkFramebuffer get() const { return framebuffer; }
    operator VkFramebuffer() const { return framebuffer; }

    bool valid() const { return framebuffer != VK_NULL_HANDLE; }

    bool create(const FrameBufferConfig& config);
    static VkFramebuffer create(VkDevice device, const FrameBufferConfig& config);

private:
    VkDevice device = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
};

}