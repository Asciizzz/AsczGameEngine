#pragma once

#include <string>
#include "AzVulk/Descriptor.hpp"
#include "Helpers/Templates.hpp"

namespace AzVulk {
class Device;
}

namespace Az3D {

// Addressing mode enum is now outside Texture, shared globally
enum class TextureAddressMode {
    Repeat            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    MirroredRepeat    = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    ClampToEdge       = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    ClampToBorder     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    MirrorClampToEdge = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE
};

// Vulkan texture resource (image + view + memory only)
struct Texture {
    std::string path;
    VkImage image         = VK_NULL_HANDLE;
    VkImageView view      = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

// Texture manager with Vulkan helpers
class TextureGroup {
public:
    TextureGroup(const AzVulk::Device* lDevice);
    ~TextureGroup();

    const AzVulk::Device* vkDevice;
    SharedPtrVec<Texture> textures;

    size_t addTexture(std::string imagePath, uint32_t mipLevels = 0);
    void createSinglePixel(uint8_t r, uint8_t g, uint8_t b); // single pixel RGB

    // Vulkan image creation helpers
    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format,
                     VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                     VkImage& image, VkDeviceMemory& imageMemory);

    void createImageView(VkImage image, VkFormat format, uint32_t mipLevels, VkImageView& imageView);

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
                               VkImageLayout newLayout, uint32_t mipLevels);

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

    // Descriptor resources
    AzVulk::DescPool   descPool;
    AzVulk::DescLayout descLayout;
    AzVulk::DescSets   descSet;

    const VkDescriptorSet       getDescSet()    const { return descSet.get(); }
    const VkDescriptorSetLayout getDescLayout() const { return descLayout.get(); }
    const VkDescriptorPool      getDescPool()   const { return descPool.get(); }

    // Shared samplers
    std::vector<VkSampler> samplers;
    std::unordered_map<TextureAddressMode, uint32_t> modeToSamplerIndex;
    uint32_t samplerPoolCount = 0;

    void createSharedSamplersFromModes();
    void cleanupSharedSamplers();
    void createDescriptorInfo();
};

} // namespace Az3D
