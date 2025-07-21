#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <memory>

namespace AzGame {
    class VulkanDevice;
    class TextureManager;
}

namespace AzModel {
    /**
     * @brief Material class that manages texture resources for 3D models
     * 
     * This class encapsulates texture management for materials, providing
     * a clean interface for binding textures to specific faces/triangles.
     * In the future, this can be extended to support multiple texture types
     * (diffuse, normal, specular, etc.) and shader parameters.
     */
    class Material {
    public:
        Material(const AzGame::VulkanDevice& device, VkCommandPool commandPool);
        ~Material();

        // Delete copy constructor and assignment operator
        Material(const Material&) = delete;
        Material& operator=(const Material&) = delete;

        /**
         * @brief Load a texture from file path
         * @param texturePath Path to the texture file
         * @return true if texture loaded successfully, false otherwise
         */
        bool loadTexture(const std::string& texturePath);

        /**
         * @brief Create a procedural checkerboard texture as fallback
         * @param size Size of the checkerboard texture (default 256x256)
         */
        void createCheckerboardTexture(uint32_t size = 256);

        /**
         * @brief Get the texture image view for binding to descriptor sets
         * @return VkImageView handle
         */
        VkImageView getTextureImageView() const { return textureImageView; }

        /**
         * @brief Get the texture sampler for binding to descriptor sets
         * @return VkSampler handle
         */
        VkSampler getTextureSampler() const { return textureSampler; }

        /**
         * @brief Check if the material has a valid texture loaded
         * @return true if texture is loaded and ready to use
         */
        bool isValid() const { return textureLoaded; }

        /**
         * @brief Get the name/identifier of this material
         * @return Material name
         */
        const std::string& getName() const { return materialName; }

        /**
         * @brief Set the name/identifier of this material
         * @param name New material name
         */
        void setName(const std::string& name) { materialName = name; }

    private:
        const AzGame::VulkanDevice& vulkanDevice;
        VkCommandPool commandPool;

        // Texture resources
        VkImage textureImage = VK_NULL_HANDLE;
        VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
        VkImageView textureImageView = VK_NULL_HANDLE;
        VkSampler textureSampler = VK_NULL_HANDLE;

        // Material properties
        std::string materialName = "DefaultMaterial";
        bool textureLoaded = false;

        // Helper methods
        void createTextureImageView();
        void createTextureSampler();
        void createImage(uint32_t width, uint32_t height, VkFormat format, 
                        VkImageTiling tiling, VkImageUsageFlags usage, 
                        VkMemoryPropertyFlags properties, VkImage& image, 
                        VkDeviceMemory& imageMemory);
        void transitionImageLayout(VkImage image, VkFormat format, 
                                  VkImageLayout oldLayout, VkImageLayout newLayout);
        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                         VkMemoryPropertyFlags properties, VkBuffer& buffer, 
                         VkDeviceMemory& bufferMemory);
        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);
        void cleanup();
    };
}
