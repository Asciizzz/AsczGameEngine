#pragma once

#include <Templates.hpp>

#include <vulkan/vulkan.h>
#include <string>

namespace tinyVk {
class Device;

struct ImageUsage {
    static constexpr VkImageUsageFlags TransferSrc  = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    static constexpr VkImageUsageFlags TransferDst  = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    static constexpr VkImageUsageFlags Sampled      = VK_IMAGE_USAGE_SAMPLED_BIT;
    static constexpr VkImageUsageFlags Storage      = VK_IMAGE_USAGE_STORAGE_BIT;
    static constexpr VkImageUsageFlags ColorAttach  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    static constexpr VkImageUsageFlags DepthStencil = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
};

struct ImageLayout {
    static constexpr VkImageLayout Undefined                  = VK_IMAGE_LAYOUT_UNDEFINED;
    static constexpr VkImageLayout General                    = VK_IMAGE_LAYOUT_GENERAL;
    static constexpr VkImageLayout Preinitialized             = VK_IMAGE_LAYOUT_PREINITIALIZED;
    static constexpr VkImageLayout PresentSrcKHR              = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    static constexpr VkImageLayout TransferSrcOptimal         = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    static constexpr VkImageLayout TransferDstOptimal         = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    static constexpr VkImageLayout ShaderReadOnlyOptimal      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    static constexpr VkImageLayout DepthReadOnlyOptimal       = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
    static constexpr VkImageLayout ColorAttachmentOptimal     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    static constexpr VkImageLayout DepthStencilReadOnlyOptimal = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    static constexpr VkImageLayout DepthStencilAttachmentOptimal = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
};

struct ImageTiling {
    static constexpr VkImageTiling Optimal = VK_IMAGE_TILING_OPTIMAL;
    static constexpr VkImageTiling Linear  = VK_IMAGE_TILING_LINEAR;
};

struct ImageAspect {
    static constexpr VkImageAspectFlags Color   = VK_IMAGE_ASPECT_COLOR_BIT;
    static constexpr VkImageAspectFlags Depth   = VK_IMAGE_ASPECT_DEPTH_BIT;
    static constexpr VkImageAspectFlags Stencil = VK_IMAGE_ASPECT_STENCIL_BIT;
    static constexpr VkImageAspectFlags Metadata= VK_IMAGE_ASPECT_METADATA_BIT;
};

// Image configuration struct for easy setup
struct ImageConfig {
    uint32_t width = 0;
    uint32_t height = 0;
    void dimensions(uint32_t w, uint32_t h) { width = w; height = h; }
    uint32_t depth = 1;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageType imageType = VK_IMAGE_TYPE_2D;
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageUsageFlags usage = 0;
    VkMemoryPropertyFlags memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkDevice device = VK_NULL_HANDLE; // Required
    VkPhysicalDevice pDevice = VK_NULL_HANDLE; // Optional, for memory type finding
};

// Image view configuration
struct ImageViewConfig {
    VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D;
    VkFormat format = VK_FORMAT_UNDEFINED;  // If UNDEFINED, uses image format
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    uint32_t baseMipLevel = 0;
    uint32_t mipLevels = VK_REMAINING_MIP_LEVELS;
    uint32_t baseArrayLayer = 0;
    uint32_t arrayLayers = VK_REMAINING_ARRAY_LAYERS;
    VkComponentMapping components = {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
    };
};

// Main ImageVk class

enum class TOwnership {
    Owned,
    External
};

class TextureVk;
class ImageVk {
public:
    ImageVk() noexcept = default;
    ~ImageVk() { cleanup(); }
    void cleanup();

    ImageVk(const ImageVk&) = delete;
    ImageVk& operator=(const ImageVk&) = delete;

    ImageVk(ImageVk&& other) noexcept;
    ImageVk& operator=(ImageVk&& other) noexcept;

    // =====================

    ImageVk& createImage(const ImageConfig& config);
    ImageVk& createView(const ImageViewConfig& viewConfig);
    ImageVk& wrapExternalImage(VkDevice device, VkImage extImage, VkFormat fmt, VkExtent2D extent);

    inline VkImage        image()  const { return image_;  }
    inline VkImageView    view()   const { return view_;   }
    inline VkDeviceMemory memory() const { return memory_; }
    inline VkFormat       format() const { return format_; }
    inline uint32_t       width()  const { return width_;  }
    inline uint32_t       height() const { return height_; }
    inline VkExtent2D extent2D()   const { return { width_, height_ }; }
    inline VkExtent3D extent3D()   const { return { width_, height_, depth_ }; }
    inline uint32_t   mipLevels()  const { return mipLevels_; }
    inline uint32_t   arrayLayers()const { return arrayLayers_; }
    inline VkImageLayout  layout() const { return layout_; }

    bool valid() const { return image_ != VK_NULL_HANDLE && memory_ != VK_NULL_HANDLE; }
    bool hasImage() const { return image_ != VK_NULL_HANDLE; }
    bool hasView() const { return view_ != VK_NULL_HANDLE; }

    void setLayout(VkImageLayout newLayout) { layout_ = newLayout; }

    // Static helper functions
    static uint32_t autoMipLevels(uint32_t width, uint32_t height);
private:
    VkDevice device_ = VK_NULL_HANDLE;

    VkImage        image_  = VK_NULL_HANDLE;
    VkImageView    view_   = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    TOwnership ownership_  = TOwnership::Owned;

    VkFormat format_ = VK_FORMAT_UNDEFINED;
    uint32_t width_  = 0;
    uint32_t height_ = 0;
    uint32_t depth_  = 1;
    uint32_t mipLevels_   = 1;
    uint32_t arrayLayers_ = 1;
    VkImageLayout layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
};


// Sampler configuration struct for easy setup
struct SamplerConfig {
    VkFilter magFilter = VK_FILTER_LINEAR;
    VkFilter minFilter = VK_FILTER_LINEAR;
    VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    void addressModes(VkSamplerAddressMode mode) {
        addressModeU = mode;
        addressModeV = mode;
        addressModeW = mode;
    }

    VkBool32 anisotropyEnable = VK_TRUE;
    float maxAnisotropy = 16.0f; // Will be clamped to device limit
    float mipLodBias = 0.0f;
    float minLod = 0.0f;
    float maxLod = VK_LOD_CLAMP_NONE;
    VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    VkBool32 unnormalizedCoordinates = VK_FALSE;
    VkBool32 compareEnable = VK_FALSE;
    VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;

    VkDevice device = VK_NULL_HANDLE; // Required
    VkPhysicalDevice pDevice = VK_NULL_HANDLE; // Optional, for anisotropy limits
};

class SamplerVk {
public:
    SamplerVk() noexcept = default;
    ~SamplerVk() { cleanup(); }
    void cleanup();

    SamplerVk(const SamplerVk&) = delete;
    SamplerVk& operator=(const SamplerVk&) = delete;

    SamplerVk(SamplerVk&& other) noexcept;
    SamplerVk& operator=(SamplerVk&& other) noexcept;

    // =====================

    void create(const SamplerConfig& config);

    VkSampler sampler() const { return sampler_; }
    operator VkSampler() const { return sampler_; }

    bool valid() const { return sampler_ != VK_NULL_HANDLE; }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;

    // Helper to clamp anisotropy to device limits
    static float getMaxAnisotropy(VkPhysicalDevice pDevice, float requested);
};


class TextureVk {
public:
    static void transitionLayout(ImageVk& image, VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout);
    static void copyFromBuffer(ImageVk& image, VkCommandBuffer cmd, VkBuffer srcBuffer);
    static void generateMipmaps(ImageVk& image, VkCommandBuffer cmd, VkPhysicalDevice pDevice);

    static VkPipelineStageFlags getStageFlags(VkImageLayout layout);
    static VkAccessFlags getAccessFlags(VkImageLayout layout);
};

}