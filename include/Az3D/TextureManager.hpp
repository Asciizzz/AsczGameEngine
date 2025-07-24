#pragma once

#include "Az3D/Texture.hpp"
#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>
#include <memory>

// Forward declaration
namespace AzVulk {
    class VulkanDevice;
}

namespace Az3D {
    
    // TextureManager - manages all textures in the Az3D system
    class TextureManager {
    public:
        TextureManager(const AzVulk::VulkanDevice& device, VkCommandPool commandPool);
        ~TextureManager();

        // Delete copy constructor and assignment operator
        TextureManager(const TextureManager&) = delete;
        TextureManager& operator=(const TextureManager&) = delete;

        // Texture management
        bool loadTexture(const std::string& textureId, const std::string& imagePath);
        bool hasTexture(const std::string& textureId) const;
        const Texture* getTexture(const std::string& textureId) const;
        
        // Remove texture from memory
        bool unloadTexture(const std::string& textureId);
        
        // Get default texture for fallback
        const Texture* getDefaultTexture() const;
        
        // Statistics
        size_t getTextureCount() const { return textures.size(); }
        std::vector<std::string> getTextureIds() const;

    private:
        const AzVulk::VulkanDevice& vulkanDevice;
        VkCommandPool commandPool;
        
        // Texture storage
        std::unordered_map<std::string, std::unique_ptr<Texture>> textures;
        std::unique_ptr<Texture> defaultTexture;
        
        // Helper methods
        std::unique_ptr<TextureData> createTextureDataFromFile(const std::string& imagePath);
        void createDefaultTexture();
        void destroyTextureData(TextureData* textureData);
        
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
    
} // namespace Az3D
