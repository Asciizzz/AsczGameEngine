#pragma once

#include <string>

#include "AzVulk/DescriptorSets.hpp"
#include "Helpers/Templates.hpp"

namespace AzVulk {
    class Device;
}

namespace Az3D {

    // Vulkan texture resource
    struct Texture {
        enum Mode {
            Repeat = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            MirroredRepeat = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
            ClampToEdge = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            ClampToBorder = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            MirrorClampToEdge = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE
        };

        std::string path;
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        bool semiTransparent = false;
        Mode addressMode = Repeat;
    };

    // Texture manager with Vulkan helpers
    class TextureManager {
    public:

        TextureManager(const AzVulk::Device* device);
        ~TextureManager();

        const AzVulk::Device* vkDevice;
        SharedPtrVec<Texture> textures;

        size_t addTexture(std::string imagePath, bool semiTransparent = false, Texture::Mode addressMode = Texture::Repeat);
        void createDefaultTexture();

        // Vulkan image creation helpers
        void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, 
                        VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, 
                        VkImage& image, VkDeviceMemory& imageMemory);
        void createImageView(VkImage image, VkFormat format, uint32_t mipLevels, VkImageView& imageView);

        void createSampler(uint32_t mipLevels, VkSampler& sampler, Texture::Mode adressMode = Texture::Repeat);

        void transitionImageLayout( VkImage image, VkFormat format, VkImageLayout oldLayout, 
                                    VkImageLayout newLayout, uint32_t mipLevels);
        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    
        void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);


        AzVulk::DynamicDescriptor dynamicDescriptor;
        void createDescriptorSets();
        VkDescriptorSet getDescriptorSet() const { return dynamicDescriptor.getSet(); }

        void uploadToGPU() {
            createDescriptorSets();
        }
    };
}
