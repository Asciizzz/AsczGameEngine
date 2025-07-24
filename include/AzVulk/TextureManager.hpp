#pragma once

#include <vulkan/vulkan.h>
#include <string>

namespace AzVulk {
    class TextureManager {
    public:
        TextureManager(const class VulkanDevice& device, VkCommandPool commandPool);
        ~TextureManager();

        // Delete copy constructor and assignment operator
        TextureManager(const TextureManager&) = delete;
        TextureManager& operator=(const TextureManager&) = delete;

        void createTextureImage(const std::string& imagePath);
        void createTextureImageView();
        void createTextureSampler();

        
        const VulkanDevice& vulkanDevice;
        VkCommandPool commandPool;
        
        uint32_t mipLevels = 1; // Number of mip levels in the texture
        VkImage textureImage = VK_NULL_HANDLE;
        VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
        VkImageView textureImageView = VK_NULL_HANDLE;
        VkSampler textureSampler = VK_NULL_HANDLE;

        // Helper methods (now public for direct access)
        void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, 
                        VkImageUsageFlags usage, VkMemoryPropertyFlags properties, 
                        VkImage& image, VkDeviceMemory& imageMemory);
        void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels = 1);
        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
        void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    };
}
