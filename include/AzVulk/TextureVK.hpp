#pragma once

#include <Helpers/Templates.hpp>

#include <vulkan/vulkan.h>
#include <string>

namespace AzVulk {
class Device;

struct ImageUsage {
    static constexpr VkImageUsageFlags TransferSrc  = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    static constexpr VkImageUsageFlags TransferDst  = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    static constexpr VkImageUsageFlags Sampled      = VK_IMAGE_USAGE_SAMPLED_BIT;
    static constexpr VkImageUsageFlags Storage      = VK_IMAGE_USAGE_STORAGE_BIT;
    static constexpr VkImageUsageFlags ColorAttach  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    static constexpr VkImageUsageFlags DepthStencil = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
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
    
    // Builder pattern methods for easy configuration
    ImageConfig& withDimensions(uint32_t w, uint32_t h, uint32_t d = 1);
    ImageConfig& withFormat(VkFormat fmt);
    ImageConfig& withUsage(VkImageUsageFlags usageFlags);
    ImageConfig& withMemProps(VkMemoryPropertyFlags memProps);
    ImageConfig& withMipLevels(uint32_t levels);
    ImageConfig& withSamples(VkSampleCountFlagBits sampleCount);
    ImageConfig& withTiling(VkImageTiling imageTiling);
    
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
    ImageViewConfig& withMipLevels(uint32_t levels);
    ImageViewConfig& withComponents(VkComponentMapping comp);

    ImageViewConfig& withAutoMipLevels(uint32_t width, uint32_t height);
};

// Main ImageVK class
class ImageVK {
public:
    ImageVK() = default;
    ImageVK(const Device* device);
    ImageVK(VkDevice lDevice, VkPhysicalDevice pDevice);

    ~ImageVK();

    // Delete copy constructor and assignment operator
    ImageVK(const ImageVK&) = delete;
    ImageVK& operator=(const ImageVK&) = delete;

    // Move constructor and assignment operator
    ImageVK(ImageVK&& other) noexcept;
    ImageVK& operator=(ImageVK&& other) noexcept;

    // Create and initialize
    ImageVK& init(const Device* device);
    ImageVK& init(VkDevice lDevice, VkPhysicalDevice pDevice);
    ImageVK& createImage(const ImageConfig& config);
    ImageVK& createView(const ImageViewConfig& viewConfig);

    // For static texture
    void transitionLayout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyFromBuffer(VkCommandBuffer cmd, VkBuffer srcBuffer);
    void generateMipmaps(VkCommandBuffer cmd);

    // Immediate operations using temporary command buffers
    ImageVK& transitionLayoutImmediate(VkCommandBuffer tempCmd, VkImageLayout oldLayout, VkImageLayout newLayout);
    ImageVK& copyFromBufferImmediate(VkCommandBuffer tempCmd, VkBuffer srcBuffer);
    ImageVK& generateMipmapsImmediate(VkCommandBuffer tempCmd);

    // Getters
    VkImage getImage() const { return image; }
    VkImageView getView() const { return view; }
    VkDeviceMemory getMemory() const { return memory; }
    VkFormat getFormat() const { return format; }
    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }
    uint32_t getMipLevels() const { return mipLevels; }
    VkImageLayout getCurrentLayout() const { return currentLayout; }

    bool isValid() const;
    void cleanup();

    // Static helper functions
    static VkFormat getVulkanFormatFromChannels(int channels);
    static std::vector<uint8_t> convertToValidData(int channels, int width, int height, const uint8_t* srcData);
    static uint32_t autoMipLevels(uint32_t width, uint32_t height);

private:
    VkDevice lDevice = VK_NULL_HANDLE;
    VkPhysicalDevice pDevice = VK_NULL_HANDLE;

    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Helper methods
    VkPipelineStageFlags getStageFlags(VkImageLayout layout);
    VkAccessFlags getAccessFlags(VkImageLayout layout);
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

    // Builder pattern methods for easy configuration
    SamplerConfig& setFilters(VkFilter magFilter, VkFilter minFilter);
    SamplerConfig& setMipmapMode(VkSamplerMipmapMode mode);
    SamplerConfig& setAddressModes(VkSamplerAddressMode mode);
    SamplerConfig& setAddressModes(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w);
    SamplerConfig& setAnisotropy(VkBool32 enable, float maxAniso = 16.0f);
    SamplerConfig& setLodRange(float minLod, float maxLod, float bias = 0.0f);
    SamplerConfig& setBorderColor(VkBorderColor color);
    SamplerConfig& setCompare(VkBool32 enable, VkCompareOp op = VK_COMPARE_OP_LESS);
};

class SamplerVK {
public:
    SamplerVK() = default;
    SamplerVK(const Device* device);
    SamplerVK(VkDevice lDevice, VkPhysicalDevice pDevice);

    ~SamplerVK();

    // Delete copy constructor and assignment operator
    SamplerVK(const SamplerVK&) = delete;
    SamplerVK& operator=(const SamplerVK&) = delete;

    // Move constructor and assignment operator
    SamplerVK(SamplerVK&& other) noexcept;
    SamplerVK& operator=(SamplerVK&& other) noexcept;

    // Create and initialize
    SamplerVK& init(const Device* device);
    SamplerVK& init(VkDevice lDevice, VkPhysicalDevice pDevice);
    SamplerVK& create(const SamplerConfig& config);

    // Getters
    VkSampler get() const { return sampler; }
    operator VkSampler() const { return sampler; } // Implicit conversion

    bool isValid() const { return sampler != VK_NULL_HANDLE; }
    void cleanup();

private:
    VkDevice lDevice = VK_NULL_HANDLE;
    VkPhysicalDevice pDevice = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;

    // Helper to clamp anisotropy to device limits
    float getMaxAnisotropy(float requested) const;
};

}