#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace AzVulk {

// Describes a single attachment in the render pass
struct AttachmentConfig {
    VkFormat              format;
    VkSampleCountFlagBits samples        = VK_SAMPLE_COUNT_1_BIT;
    VkAttachmentLoadOp    loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentStoreOp   storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    VkAttachmentLoadOp    stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp   stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    VkImageLayout         initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout         finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    // Helper constructors
    static AttachmentConfig createColorAttachment(VkFormat format, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    static AttachmentConfig createDepthAttachment(VkFormat format = VK_FORMAT_D32_SFLOAT, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    static AttachmentConfig createResolveAttachment(VkFormat format, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
};

// Describes which attachments a subpass uses and how
struct SubpassConfig {
    std::vector<uint32_t> colorAttachmentIndices;     // Indices into the attachments array
    std::vector<uint32_t> inputAttachmentIndices;     // For reading from previous subpasses
    std::vector<uint32_t> resolveAttachmentIndices;   // For MSAA resolve (must match colorAttachmentIndices size)
    uint32_t              depthAttachmentIndex = VK_ATTACHMENT_UNUSED;
    bool                  preserveAttachments = false; // Whether to preserve attachments not explicitly referenced
    
    // Helper constructors
    static SubpassConfig createSimpleSubpass(uint32_t colorAttachment, uint32_t depthAttachment = VK_ATTACHMENT_UNUSED);
    static SubpassConfig createMRTSubpass(const std::vector<uint32_t>& colorAttachments, uint32_t depthAttachment = VK_ATTACHMENT_UNUSED);
};

struct RenderPassConfig {
    std::vector<AttachmentConfig>   attachments;
    std::vector<SubpassConfig>      subpasses;
    std::vector<VkSubpassDependency> dependencies;
    
    // Factory methods for common configurations
    static RenderPassConfig createForwardRenderingConfig(VkFormat swapChainFormat);
    static RenderPassConfig createDeferredGBufferConfig();
    static RenderPassConfig createShadowMapConfig();
    static RenderPassConfig createPostProcessConfig(VkFormat colorFormat);
    static RenderPassConfig createMSAAConfig(VkFormat colorFormat, VkFormat resolveFormat, VkSampleCountFlagBits samples);
    
    // Helper methods
    void addDefaultDependency();
    void addDependency(uint32_t srcSubpass, uint32_t dstSubpass, 
                        VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                        VkAccessFlags srcAccess, VkAccessFlags dstAccess);
};

class RenderPass {
public:
    RenderPass(VkDevice lDevice, VkPhysicalDevice pDevice, const RenderPassConfig& config);
    ~RenderPass();
    
    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;
    
    void recreate(const RenderPassConfig& newConfig);

    VkDevice lDevice;
    VkPhysicalDevice pDevice;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    RenderPassConfig config;

    VkRenderPass get() const { return renderPass; }
    
    // Getters for attachment info
    uint32_t getAttachmentCount() const { return static_cast<uint32_t>(config.attachments.size()); }
    uint32_t getSubpassCount() const { return static_cast<uint32_t>(config.subpasses.size()); }
    const AttachmentConfig& getAttachment(uint32_t index) const { return config.attachments[index]; }

private:
    void createRenderPass();
    void cleanup();
    
    // Helper methods for creating Vulkan structures
    std::vector<VkAttachmentDescription> createAttachmentDescriptions() const;
    std::vector<VkSubpassDescription> createSubpassDescriptions(
        std::vector<std::vector<VkAttachmentReference>>& colorRefs,
        std::vector<std::vector<VkAttachmentReference>>& inputRefs,
        std::vector<std::vector<VkAttachmentReference>>& resolveRefs,
        std::vector<VkAttachmentReference>& depthRefs
    ) const;
};

} // namespace AzVulk
