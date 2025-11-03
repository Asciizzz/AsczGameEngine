#include "tinyVk/Render/FrameBuffer.hpp"
#include <stdexcept>

using namespace tinyVk;

FrameBufferConfig& FrameBufferConfig::withRenderPass(VkRenderPass rp) {
    renderPass = rp;
    return *this;
}

FrameBufferConfig& FrameBufferConfig::addAttachment(VkImageView att) {
    attachments.push_back(att);
    return *this;
}

FrameBufferConfig& FrameBufferConfig::addAttachments(const std::vector<VkImageView>& atts) {
    for (auto att : atts) addAttachment(att);
    return *this;
}

FrameBufferConfig& FrameBufferConfig::withExtent(VkExtent2D ext) {
    extent = ext;
    return *this;
}

FrameBufferConfig& FrameBufferConfig::withExtent(uint32_t width, uint32_t height) {
    extent = {width, height};
    return *this;
}

FrameBufferConfig& FrameBufferConfig::withLayers(uint32_t layers) {
    this->layers = layers;
    return *this;
}



FrameBuffer::~FrameBuffer() {
    cleanup();
}
void FrameBuffer::cleanup() {
    if (device != VK_NULL_HANDLE && framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
        framebuffer = VK_NULL_HANDLE;
    }
}

FrameBuffer::FrameBuffer(FrameBuffer&& other) noexcept
    : device(other.device)
    , framebuffer(other.framebuffer) {
    
    // Reset other object
    other.device = VK_NULL_HANDLE;
    other.framebuffer = VK_NULL_HANDLE;
}
FrameBuffer& FrameBuffer::operator=(FrameBuffer&& other) noexcept {
    if (this != &other) {
        cleanup();

        device = other.device;
        framebuffer = other.framebuffer;

        // Reset other object
        other.device = VK_NULL_HANDLE;
        other.framebuffer = VK_NULL_HANDLE;
    }
    return *this;
}


bool FrameBuffer::create(const FrameBufferConfig& config) {
    cleanup();

    this->device = device;

    framebuffer = create(device, config);

    return true;
}


VkFramebuffer FrameBuffer::create(VkDevice device, const FrameBufferConfig& config) {
    VkFramebuffer framebuffer = VK_NULL_HANDLE;

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = config.renderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(config.attachments.size());
    framebufferInfo.pAttachments = config.attachments.data();
    framebufferInfo.width = config.extent.width;
    framebufferInfo.height = config.extent.height;
    framebufferInfo.layers = config.layers;

    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create framebuffer!");
        return VK_NULL_HANDLE;
    }

    return framebuffer;
}