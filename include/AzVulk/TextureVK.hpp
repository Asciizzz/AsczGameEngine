#pragma once

#include <Helpers/Templates.hpp>

#include <vulkan/vulkan.h>
#include <string>

namespace AzVulk {
    class Device;

    struct ImageUsageAlias {
        static constexpr VkImageUsageFlags TransferSrc  = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        static constexpr VkImageUsageFlags TransferDst  = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        static constexpr VkImageUsageFlags Sampled      = VK_IMAGE_USAGE_SAMPLED_BIT;
        static constexpr VkImageUsageFlags Storage      = VK_IMAGE_USAGE_STORAGE_BIT;
        static constexpr VkImageUsageFlags ColorAttach  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
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
        ImageConfig& setDimensions(uint32_t w, uint32_t h, uint32_t d = 1);
        ImageConfig& setFormat(VkFormat fmt);
        ImageConfig& setUsage(VkImageUsageFlags usageFlags);
        ImageConfig& setMemProps(VkMemoryPropertyFlags memProps);
        ImageConfig& setMipLevels(uint32_t levels);
        ImageConfig& setAutoMipLevels(uint32_t width, uint32_t height);
        ImageConfig& setSamples(VkSampleCountFlagBits sampleCount);
        ImageConfig& setTiling(VkImageTiling imageTiling);
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
        ImageViewConfig& setType(VkImageViewType viewType);
        ImageViewConfig& setFormat(VkFormat fmt);
        ImageViewConfig& setAspectMask(VkImageAspectFlags aspect);
        ImageViewConfig& setMipLevels(uint32_t levels);
        ImageViewConfig& setAutoMipLevels(uint32_t width, uint32_t height);
    };

    // Main ImageVK class
    class ImageVK {
    public:
        ImageVK() = default; void init(const Device* device);
        ImageVK(const Device* device) : device(device) {}
        ~ImageVK();

        // Delete copy constructor and assignment operator
        ImageVK(const ImageVK&) = delete;
        ImageVK& operator=(const ImageVK&) = delete;

        // Move constructor and assignment operator
        ImageVK(ImageVK&& other) noexcept;
        ImageVK& operator=(ImageVK&& other) noexcept;

        // Creation methods
        bool createImage(const ImageConfig& config);
        bool createImageView(const ImageViewConfig& viewConfig);

        // For static texture
        void transitionLayout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout,
                            uint32_t baseMipLevel = 0, uint32_t mipLevels = VK_REMAINING_MIP_LEVELS,
                            uint32_t baseArrayLayer = 0, uint32_t arrayLayers = VK_REMAINING_ARRAY_LAYERS);
        void copyFromBuffer(VkCommandBuffer cmd, VkBuffer srcBuffer, 
                            uint32_t width, uint32_t height, uint32_t mipLevel = 0);
        void generateMipmaps(VkCommandBuffer cmd);

        // Immediate operations using temporary command buffers
        ImageVK& transitionLayoutImmediate(VkImageLayout oldLayout, VkImageLayout newLayout);
        ImageVK& copyFromBufferImmediate(VkBuffer srcBuffer, uint32_t width, uint32_t height, uint32_t mipLevel = 0);
        ImageVK& generateMipmapsImmediate();

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
        const Device* device;
        
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
        
        std::string debugName;

        // Helper methods
        VkPipelineStageFlags getStageFlags(VkImageLayout layout);
        VkAccessFlags getAccessFlags(VkImageLayout layout);
    };

    // Legacy-style static utility functions for backward compatibility

    void createImage(const Device* device, uint32_t width, uint32_t height, VkFormat format, 
                    VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                    VkImage& image, VkDeviceMemory& imageMemory);
    VkImageView createImageView(const Device* device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
}