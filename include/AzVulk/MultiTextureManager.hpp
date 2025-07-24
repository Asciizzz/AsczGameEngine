#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>
#include <memory>

namespace AzVulk {
    
    // Single texture resource
    struct TextureResource {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        uint32_t mipLevels = 1;
        uint32_t width = 0;
        uint32_t height = 0;
        
        bool isValid() const {
            return image != VK_NULL_HANDLE && memory != VK_NULL_HANDLE && 
                   view != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE;
        }
    };
    
    // Enhanced TextureManager for multiple textures
    class TextureManager {
    public:
        TextureManager(const class VulkanDevice& device, VkCommandPool commandPool);
        ~TextureManager();

        // Delete copy constructor and assignment operator
        TextureManager(const TextureManager&) = delete;
        TextureManager& operator=(const TextureManager&) = delete;

        // Enhanced texture management
        bool loadTexture(const std::string& textureId, const std::string& imagePath);
        bool hasTexture(const std::string& textureId) const;
        const TextureResource* getTexture(const std::string& textureId) const;
        
        // Get default texture for fallback
        const TextureResource* getDefaultTexture() const;
        
        // Legacy single texture support (for backward compatibility)
        void createTextureImage(const std::string& imagePath);
        void createTextureImageView();
        void createTextureSampler();
        
        // Public access to legacy texture (for backward compatibility)
        VkImage textureImage = VK_NULL_HANDLE;
        VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
        VkImageView textureImageView = VK_NULL_HANDLE;
        VkSampler textureSampler = VK_NULL_HANDLE;
        uint32_t mipLevels = 1;

    private:
        const class VulkanDevice& vulkanDevice;
        VkCommandPool commandPool;
        
        // Texture storage
        std::unordered_map<std::string, std::unique_ptr<TextureResource>> textures;
        std::unique_ptr<TextureResource> defaultTexture;
        
        // Helper methods
        std::unique_ptr<TextureResource> createTextureFromFile(const std::string& imagePath);
        void createDefaultTexture();
        void destroyTextureResource(TextureResource* texture);
        
        // Vulkan helper methods
        void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, 
                        VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, 
                        VkImage& image, VkDeviceMemory& imageMemory);
        void createImageView(VkImage image, VkFormat format, uint32_t mipLevels, VkImageView& imageView);
        void createSampler(uint32_t mipLevels, VkSampler& sampler);
        void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, 
                                  VkImageLayout newLayout, uint32_t mipLevels = 1);
        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
        void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    };
    
} // namespace AzVulk
