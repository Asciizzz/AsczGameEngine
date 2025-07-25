#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace AzVulk {
    class Device;
}

namespace Az3D {
    // Barebone Vulkan texture resource
    struct Texture {
        std::string path;
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
    };

    // Barebone manager for textures
    class TextureManager {
    public:
        const AzVulk::Device& vulkanDevice;
        VkCommandPool commandPool;
        std::vector<Texture> textures;

        TextureManager(const AzVulk::Device& device, VkCommandPool pool);
        ~TextureManager();
        
        size_t addTexture(const char* imagePath);
        void createDefaultTexture();

    private:
        // Vulkan helper methods
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
