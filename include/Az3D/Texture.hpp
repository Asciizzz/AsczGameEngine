#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace AzVulk {
    class Device;
}

namespace Az3D {
    // Vulkan texture resource
    struct Texture {
        std::string path;
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        bool semiTransparent = false;
    };

    // Texture manager with Vulkan helpers
    class TextureManager {
    public:
        const AzVulk::Device& vulkanDevice;
        VkCommandPool commandPool;

        size_t count = 0; // Track the number of textures
        std::vector<Texture> textures;

        TextureManager(const AzVulk::Device& device, VkCommandPool pool);
        ~TextureManager();
        
        size_t addTexture(std::string imagePath, bool semiTransparent = false);
        void createDefaultTexture(); // fallback for missing assets

        // Vulkan image creation helpers
        void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, 
                        VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, 
                        VkImage& image, VkDeviceMemory& imageMemory);
        void createImageView(VkImage image, VkFormat format, uint32_t mipLevels, VkImageView& imageView);
        void createSampler(uint32_t mipLevels, VkSampler& sampler);
        void transitionImageLayout( VkImage image, VkFormat format, VkImageLayout oldLayout, 
                                    VkImageLayout newLayout, uint32_t mipLevels);
        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
        void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    };
}
