#pragma once

#include <vulkan/vulkan.h>
#include <Helpers/Templates.hpp>
#include <string>

namespace AzVulk {
    class Device;

    // Image usage presets for common scenarios
    struct ImageUsagePreset {
        static constexpr VkImageUsageFlags DEPTH_BUFFER = 
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        
        static constexpr VkImageUsageFlags TEXTURE = 
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        
        static constexpr VkImageUsageFlags RENDER_TARGET = 
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        
        static constexpr VkImageUsageFlags COMPUTE_STORAGE = 
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        
        static constexpr VkImageUsageFlags POST_PROCESS = 
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | 
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    };

    // Memory property presets
    struct MemoryPreset {
        static constexpr VkMemoryPropertyFlags DEVICE_LOCAL = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        static constexpr VkMemoryPropertyFlags HOST_VISIBLE = 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
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
        VkMemoryPropertyFlags memoryProperties = MemoryPreset::DEVICE_LOCAL;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        
        // Builder pattern methods for easy configuration
        ImageConfig& setDimensions(uint32_t w, uint32_t h, uint32_t d = 1);
        ImageConfig& setFormat(VkFormat fmt);
        ImageConfig& setUsage(VkImageUsageFlags usageFlags);
        ImageConfig& setMemoryProperties(VkMemoryPropertyFlags memProps);
        ImageConfig& setMipLevels(uint32_t levels);
        ImageConfig& setSamples(VkSampleCountFlagBits sampleCount);
        ImageConfig& setTiling(VkImageTiling imageTiling);
        
        // Preset configurations
        static ImageConfig createDepthBuffer(uint32_t width, uint32_t height, VkFormat depthFormat);
        static ImageConfig createTexture(uint32_t width, uint32_t height, VkFormat format, uint32_t mipLevels = 1);
        static ImageConfig createRenderTarget(uint32_t width, uint32_t height, VkFormat format);
        static ImageConfig createComputeStorage(uint32_t width, uint32_t height, VkFormat format);
        static ImageConfig createPostProcessBuffer(uint32_t width, uint32_t height);
    };

    // Image view configuration
    struct ImageViewConfig {
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
        VkFormat format = VK_FORMAT_UNDEFINED;  // If UNDEFINED, uses image format
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        uint32_t baseMipLevel = 0;
        uint32_t mipLevels = VK_REMAINING_MIP_LEVELS;
        uint32_t baseArrayLayer = 0;
        uint32_t arrayLayers = VK_REMAINING_ARRAY_LAYERS;
        VkComponentMapping components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        };
        
        // Preset configurations
        static ImageViewConfig createDefault(VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);
        static ImageViewConfig createDepthView();
        static ImageViewConfig createColorView(uint32_t mipLevels = 1);
        static ImageViewConfig createCubeMapView();
    };

    // Main ImageWrapper class
    class ImageWrapper {
    public:
        ImageWrapper(const Device* device);
        ~ImageWrapper();

        // Delete copy constructor and assignment operator
        ImageWrapper(const ImageWrapper&) = delete;
        ImageWrapper& operator=(const ImageWrapper&) = delete;

        // Move constructor and assignment operator
        ImageWrapper(ImageWrapper&& other) noexcept;
        ImageWrapper& operator=(ImageWrapper&& other) noexcept;

        // Creation methods
        bool create(const ImageConfig& config);
        bool createImageView(const ImageViewConfig& viewConfig);
        
        // Convenience creation methods
        bool createDepthBuffer(uint32_t width, uint32_t height, VkFormat depthFormat);
        bool createTexture(uint32_t width, uint32_t height, VkFormat format, uint32_t mipLevels = 1);
        bool createTexture(uint32_t width, uint32_t height, int channels, const uint8_t* data, VkBuffer stagingBuffer);
        bool createRenderTarget(uint32_t width, uint32_t height, VkFormat format);
        bool createComputeStorage(uint32_t width, uint32_t height, VkFormat format);
        bool createPostProcessBuffer(uint32_t width, uint32_t height);

        // Layout transition
        void transitionLayout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout,
                            uint32_t baseMipLevel = 0, uint32_t mipLevels = VK_REMAINING_MIP_LEVELS,
                            uint32_t baseArrayLayer = 0, uint32_t arrayLayers = VK_REMAINING_ARRAY_LAYERS);
        
        // Immediate layout transition (creates temporary command buffer) - fluent interface
        ImageWrapper& transitionLayoutImmediate(VkImageLayout oldLayout, VkImageLayout newLayout);

        // Copy operations
        void copyFromBuffer(VkCommandBuffer cmd, VkBuffer srcBuffer, 
                            uint32_t width, uint32_t height, uint32_t mipLevel = 0);
        ImageWrapper& copyFromBufferImmediate(VkBuffer srcBuffer, uint32_t width, uint32_t height, uint32_t mipLevel = 0);

        // Generate mipmaps
        void generateMipmaps(VkCommandBuffer cmd);
        ImageWrapper& generateMipmapsImmediate();

        // Getters
        VkImage getImage() const { return image; }
        VkImageView getImageView() const { return imageView; }
        VkDeviceMemory getMemory() const { return memory; }
        VkFormat getFormat() const { return format; }
        uint32_t getWidth() const { return width; }
        uint32_t getHeight() const { return height; }
        uint32_t getMipLevels() const { return mipLevels; }
        VkImageLayout getCurrentLayout() const { return currentLayout; }

        // Utility methods
        bool isValid() const;
        void cleanup();
        
        // Debug information
        void setDebugName(const std::string& name);

        // Static helper functions
        static VkFormat getVulkanFormatFromChannels(int channels);
        static std::vector<uint8_t> convertTextureDataForVulkan(int channels, int width, int height, const uint8_t* srcData);
        static uint32_t autoMipLevels(uint32_t width, uint32_t height);

    private:
        const Device* device;
        
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        
        VkFormat format = VK_FORMAT_UNDEFINED;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 1;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
        VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        
        std::string debugName;

        // Helper methods
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        bool hasStencilComponent(VkFormat format);
        VkPipelineStageFlags getStageFlags(VkImageLayout layout);
        VkAccessFlags getAccessFlags(VkImageLayout layout);
    };

    // RAII helper for temporary image operations
    class TemporaryImage {
    public:
        TemporaryImage(const Device* device, const ImageConfig& config);
        ~TemporaryImage();

        ImageWrapper& get() { return image; }
        const ImageWrapper& get() const { return image; }

    private:
        ImageWrapper image;
    };

    // Static utility functions for backward compatibility with manual VkImage management
    void createImage(const Device* device, uint32_t width, uint32_t height, VkFormat format, 
                    VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                    VkImage& image, VkDeviceMemory& imageMemory);
    VkImageView createImageView(const Device* device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

    // Factory functions for common use cases
    namespace ImageFactory {
        UniquePtr<ImageWrapper> createDepthBuffer(const Device* device, uint32_t width, uint32_t height, VkFormat depthFormat);
        UniquePtr<ImageWrapper> createTexture(const Device* device, uint32_t width, uint32_t height, VkFormat format, uint32_t mipLevels = 1);
        UniquePtr<ImageWrapper> createRenderTarget(const Device* device, uint32_t width, uint32_t height, VkFormat format);
        UniquePtr<ImageWrapper> createComputeStorage(const Device* device, uint32_t width, uint32_t height, VkFormat format);
        UniquePtr<ImageWrapper> createPostProcessBuffer(const Device* device, uint32_t width, uint32_t height);
    }
}