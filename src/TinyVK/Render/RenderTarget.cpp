#include "TinyVK/Render/RenderTarget.hpp"
#include "TinyVK/Resource/TextureVK.hpp"

using namespace TinyVK;

// RenderAttachment constructors
RenderAttachment::RenderAttachment() = default;

RenderAttachment::RenderAttachment(VkImage img, VkImageView v) 
    : image(img), view(v) {}

RenderAttachment::RenderAttachment(VkImage img, VkImageView v, VkClearValue clear) 
    : image(img), view(v), clearValue(clear) {}

RenderAttachment::RenderAttachment(const ImageVK& imageVK, VkClearValue clear) 
    : image(imageVK.getImage()), view(imageVK.getView()), clearValue(clear) {}

// RenderTarget constructors
RenderTarget::RenderTarget() = default;

RenderTarget::RenderTarget(VkRenderPass renderPass, VkFramebuffer framebuffer, VkExtent2D extent)
    : renderPass(renderPass), framebuffer(framebuffer), extent(extent) {}

RenderTarget::RenderTarget(VkRenderPass renderPass, VkFramebuffer framebuffer, VkExtent2D extent,
                          std::vector<RenderAttachment> attachments)
    : renderPass(renderPass), framebuffer(framebuffer), extent(extent), 
      attachments(std::move(attachments)) {}

void RenderTarget::beginRenderPass(VkCommandBuffer cmd, VkSubpassContents contents) const {
    if (!isValid()) return;
    
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = extent;

    // Setup clear values from attachments
    std::vector<VkClearValue> clearValues;
    clearValues.reserve(attachments.size());
    for (const auto& attachment : attachments) {
        clearValues.push_back(attachment.clearValue);
    }

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.empty() ? nullptr : clearValues.data();

    vkCmdBeginRenderPass(cmd, &renderPassInfo, contents);
}

void RenderTarget::endRenderPass(VkCommandBuffer cmd) const {
    vkCmdEndRenderPass(cmd);
}

void RenderTarget::setViewportAndScissor(VkCommandBuffer cmd) const {
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void RenderTarget::setViewport(VkCommandBuffer cmd, const VkViewport& viewport) const {
    vkCmdSetViewport(cmd, 0, 1, &viewport);
}

void RenderTarget::setScissor(VkCommandBuffer cmd, const VkRect2D& scissor) const {
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}