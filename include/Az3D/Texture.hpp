#pragma once

#include <string>

#include "AzVulk/Descriptor.hpp"
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
    Mode addressMode = Repeat;
};

// Texture manager with Vulkan helpers
class TextureGroup {
public:

    TextureGroup(const AzVulk::Device* lDevice);
    ~TextureGroup();

    const AzVulk::Device* vkDevice;
    SharedPtrVec<Texture> textures;

    size_t addTexture(std::string imagePath, Texture::Mode addressMode = Texture::Repeat, uint32_t mipLevels = 0);
    void createSinglePixel(uint8_t r, uint8_t g, uint8_t b); // No A, since it's a single pixel

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
