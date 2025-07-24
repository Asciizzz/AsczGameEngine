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
    
    // TextureManager - manages all textures in the Az3D system with public access
    class TextureManager {
    public:
        TextureManager(const AzVulk::VulkanDevice& device, VkCommandPool commandPool);
        ~TextureManager();

        // Delete copy constructor and assignment operator
        TextureManager(const TextureManager&) = delete;
        TextureManager& operator=(const TextureManager&) = delete;

        // Index-based texture management
        size_t loadTexture(const std::string& imagePath);  // Returns index
        
        // Public data members for direct access
        const AzVulk::VulkanDevice& vulkanDevice;
        VkCommandPool commandPool;
        
        // Index-based texture storage (index 0 is always default texture)
        std::vector<std::unique_ptr<Texture>> textures;
        
        // Simple inline getters for compatibility
        bool hasTexture(size_t index) const { return index < textures.size() && textures[index] != nullptr; }
        const Texture* getTexture(size_t index) const { return index < textures.size() ? textures[index].get() : nullptr; }
        Texture* getTexture(size_t index) { return index < textures.size() ? textures[index].get() : nullptr; }
        size_t getTextureCount() const { return textures.size(); }
        
    private:
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
        void transitionImageLayout( VkImage image, VkFormat format, VkImageLayout oldLayout, 
                                    VkImageLayout newLayout, uint32_t mipLevels = 1);
        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
        void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    };
    
} // namespace Az3D
