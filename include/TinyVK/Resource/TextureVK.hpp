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
    uint32_t depth = 1;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageType imageType = VK_IMAGE_TYPE_2D;
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageUsageFlags usage = 0;
    VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkPhysicalDevice pDevice = VK_NULL_HANDLE; // Optional, for memory type finding
    
    // Builder pattern methods for easy configuration
    ImageConfig& withDimensions(uint32_t w, uint32_t h, uint32_t d = 1);
    ImageConfig& withFormat(VkFormat fmt);
    ImageConfig& withUsage(VkImageUsageFlags usageFlags);
    ImageConfig& withMemProps(VkMemoryPropertyFlags memProps);
    ImageConfig& withMipLevels(uint32_t levels);
    ImageConfig& withSamples(VkSampleCountFlagBits sampleCount);
    ImageConfig& withTiling(VkImageTiling imageTiling);
    ImageConfig& withPhysicalDevice(VkPhysicalDevice pDevice);

    ImageConfig& withAutoMipLevels();
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

    // You generally only need these setters
    ImageViewConfig& withType(VkImageViewType viewType);
    ImageViewConfig& withFormat(VkFormat fmt);
    ImageViewConfig& withAspectMask(VkImageAspectFlags aspect);
    ImageViewConfig& withBaseMipLevel(uint32_t baseLevel);
    ImageViewConfig& withMipLevels(uint32_t levels);
    ImageViewConfig& withBaseArrayLayer(uint32_t baseLayer);
    ImageViewConfig& withArrayLayers(uint32_t layers);
    ImageViewConfig& withComponents(VkComponentMapping comp);

    ImageViewConfig& withAutoMipLevels(uint32_t width, uint32_t height);
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
    ImageVk(VkDevice device) : device(device) {}
    ImageVk& init(VkDevice device) {
        this->device = device;
        return *this;
    }

    ~ImageVk() { cleanup(); }
    void cleanup();

    ImageVk(const ImageVk&) = delete;
    ImageVk& operator=(const ImageVk&) = delete;

    ImageVk(ImageVk&& other) noexcept;
    ImageVk& operator=(ImageVk&& other) noexcept;

    // =====================

    ImageVk& createImage(const ImageConfig& config);
    ImageVk& createView(const ImageViewConfig& viewConfig);

    ImageVk& wrapExternalImage(VkImage extImage, VkFormat fmt, VkExtent2D extent);

    VkImage getImage() const { return image; }
    VkImageView getView() const { return view; }
    VkDeviceMemory getMemory() const { return memory; }
    VkFormat getFormat() const { return format; }
    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }
    VkExtent2D getExtent2D() const { return { width, height }; }
    VkExtent3D getExtent3D() const { return { width, height, depth }; }
    uint32_t getMipLevels() const { return mipLevels; }
    uint32_t getArrayLayers() const { return arrayLayers; }
    VkImageLayout getCurrentLayout() const { return layout; }

    bool valid() const { return image != VK_NULL_HANDLE && memory != VK_NULL_HANDLE; }
    bool hasImage() const { return image != VK_NULL_HANDLE; }
    bool hasView() const { return view != VK_NULL_HANDLE; }

    // Static helper functions
    static uint32_t autoMipLevels(uint32_t width, uint32_t height);

private:
    friend class TextureVk;

    VkDevice device = VK_NULL_HANDLE;

    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    TOwnership ownership = TOwnership::Owned;

    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

    void setLayout(VkImageLayout newLayout) { layout = newLayout; } // For TextureVk
};


// Sampler configuration struct for easy setup
struct SamplerConfig {
    VkFilter magFilter = VK_FILTER_LINEAR;
    VkFilter minFilter = VK_FILTER_LINEAR;
    VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkBool32 anisotropyEnable = VK_TRUE;
    float maxAnisotropy = 16.0f; // Will be clamped to device limit
    float mipLodBias = 0.0f;
    float minLod = 0.0f;
    float maxLod = VK_LOD_CLAMP_NONE;
    VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    VkBool32 unnormalizedCoordinates = VK_FALSE;
    VkBool32 compareEnable = VK_FALSE;
    VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;

    VkPhysicalDevice pDevice = VK_NULL_HANDLE; // Optional, for anisotropy limits

    // Builder pattern methods for easy configuration
    SamplerConfig& withFilters(VkFilter magFilter, VkFilter minFilter);
    SamplerConfig& withMipmapMode(VkSamplerMipmapMode mode);
    SamplerConfig& withAddressModes(VkSamplerAddressMode mode);
    SamplerConfig& withAddressModes(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w);
    SamplerConfig& withAnisotropy(VkBool32 enable, float maxAniso = 16.0f);
    SamplerConfig& withLodRange(float minLod, float maxLod, float bias = 0.0f);
    SamplerConfig& withBorderColor(VkBorderColor color);
    SamplerConfig& withCompare(VkBool32 enable, VkCompareOp op = VK_COMPARE_OP_LESS);
    SamplerConfig& withPhysicalDevice(VkPhysicalDevice pDevice);
};

class SamplerVk {
public:
    SamplerVk() noexcept = default;
    SamplerVk(VkDevice device) : device(device) {}
    SamplerVk& init(VkDevice device) {
        this->device = device;
        return *this;
    }

    ~SamplerVk() { cleanup(); }
    void cleanup();

    SamplerVk(const SamplerVk&) = delete;
    SamplerVk& operator=(const SamplerVk&) = delete;

    SamplerVk(SamplerVk&& other) noexcept;
    SamplerVk& operator=(SamplerVk&& other) noexcept;

    // =====================

    SamplerVk& create(const SamplerConfig& config);
    SamplerVk& set(VkSampler sampler);

    VkSampler get() const { return sampler; }
    operator VkSampler() const { return sampler; } // Implicit conversion

    bool valid() const { return sampler != VK_NULL_HANDLE; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    
    TOwnership ownership = TOwnership::Owned;

    // Helper to clamp anisotropy to device limits
    static float getMaxAnisotropy(VkPhysicalDevice pDevice, float requested);
};


class TextureVk {
public:
    TextureVk() noexcept = default;
    TextureVk(VkDevice device) : image(device), sampler(device) {}
    TextureVk& init(VkDevice device) {
        image.init(device);
        sampler.init(device);
        return *this;
    }

    TextureVk(const TextureVk&) = delete;
    TextureVk& operator=(const TextureVk&) = delete;

    TextureVk(TextureVk&& other) noexcept;
    TextureVk& operator=(TextureVk&& other) noexcept;

    // =====================

    TextureVk& createImage(const ImageConfig& config);
    TextureVk& createView(const ImageViewConfig& viewConfig);
    TextureVk& createSampler(const SamplerConfig& config);

    TextureVk& setSampler(VkSampler sampler);


    VkImage getImage() const { return image.getImage(); }
    VkImageView getView() const { return image.getView(); }
    VkSampler getSampler() const { return sampler.get(); }


    // void transitionLayout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout);
    // void copyFromBuffer(VkCommandBuffer cmd, VkBuffer srcBuffer);
    // void generateMipmaps(VkCommandBuffer cmd, VkPhysicalDevice pDevice);

    // Immediate operations using temporary command buffers
    // TextureVk& transitionLayoutImmediate(VkCommandBuffer tempCmd, VkImageLayout oldLayout, VkImageLayout newLayout);
    // TextureVk& copyFromBufferImmediate(VkCommandBuffer tempCmd, VkBuffer srcBuffer);
    // TextureVk& generateMipmapsImmediate(VkCommandBuffer tempCmd, VkPhysicalDevice pDevice);

    static void transitionLayout(ImageVk& image, VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout);
    static void copyFromBuffer(ImageVk& image, VkCommandBuffer cmd, VkBuffer srcBuffer);
    static void generateMipmaps(ImageVk& image, VkCommandBuffer cmd, VkPhysicalDevice pDevice);

    bool valid() const { return image.valid() && sampler.valid(); }

private:
    ImageVk image;
    SamplerVk sampler;
    
    // Helper methods
    static VkPipelineStageFlags getStageFlags(VkImageLayout layout);
    static VkAccessFlags getAccessFlags(VkImageLayout layout);
};

}