#include "TinyVK/Resource/TextureVK.hpp"
#include "TinyVK/System/CmdBuffer.hpp"

#include <stdexcept>
#include <iostream>
#include <algorithm>

using namespace TinyVK;

// ImageConfig implementation
ImageConfig& ImageConfig::withDimensions(uint32_t w, uint32_t h, uint32_t d) {
    width = w;
    height = h;
    depth = d;
    return *this;
}

ImageConfig& ImageConfig::withAutoMipLevels() {
    mipLevels = ImageVK::autoMipLevels(width, height);
    return *this;
}

ImageConfig& ImageConfig::withFormat(VkFormat fmt) {
    format = fmt;
    return *this;
}

ImageConfig& ImageConfig::withUsage(VkImageUsageFlags usageFlags) {
    usage = usageFlags;
    return *this;
}

ImageConfig& ImageConfig::withMemProps(VkMemoryPropertyFlags memProps) {
    memoryProperties = memProps;
    return *this;
}

ImageConfig& ImageConfig::withMipLevels(uint32_t levels) {
    mipLevels = levels;
    return *this;
}

ImageConfig& ImageConfig::withSamples(VkSampleCountFlagBits sampleCount) {
    samples = sampleCount;
    return *this;
}

ImageConfig& ImageConfig::withTiling(VkImageTiling imageTiling) {
    tiling = imageTiling;
    return *this;
}

ImageConfig& ImageConfig::withPhysicalDevice(VkPhysicalDevice pDevice) {
    this->pDevice = pDevice;
    return *this;
}

// ImageViewConfig implementation
ImageViewConfig& ImageViewConfig::withType(VkImageViewType viewType) {
    type = viewType;
    return *this;
}

ImageViewConfig& ImageViewConfig::withFormat(VkFormat fmt) {
    format = fmt;
    return *this;
}

ImageViewConfig& ImageViewConfig::withAspectMask(VkImageAspectFlags aspect) {
    aspectMask = aspect;
    return *this;
}

ImageViewConfig& ImageViewConfig::withBaseMipLevel(uint32_t baseLevel) {
    baseMipLevel = baseLevel;
    return *this;
}

ImageViewConfig& ImageViewConfig::withMipLevels(uint32_t levels) {
    mipLevels = levels;
    return *this;
}

ImageViewConfig& ImageViewConfig::withBaseArrayLayer(uint32_t baseLayer) {
    baseArrayLayer = baseLayer;
    return *this;
}

ImageViewConfig& ImageViewConfig::withArrayLayers(uint32_t layers) {
    arrayLayers = layers;
    return *this;
}

ImageViewConfig& ImageViewConfig::withComponents(VkComponentMapping comp) {
    components = comp;
    return *this;
}

ImageViewConfig& ImageViewConfig::withAutoMipLevels(uint32_t width, uint32_t height) {
    mipLevels = ImageVK::autoMipLevels(width, height);
    return *this;
}





ImageVK::ImageVK(VkDevice lDevice) {
    init(lDevice);
}
ImageVK& ImageVK::init(VkDevice lDevice) {
    this->lDevice = lDevice;
    return *this;
}

ImageVK::ImageVK(const Device* device) {
    init(device);
}
ImageVK& ImageVK::init(const Device* device) {
    if (device) this->lDevice = device->lDevice;
    return *this;
}

void ImageVK::cleanup() {
    if (view != VK_NULL_HANDLE) vkDestroyImageView(lDevice, view, nullptr);

    if (ownership == Ownership::Owned) {
        if (image != VK_NULL_HANDLE)  vkDestroyImage(lDevice, image, nullptr);
        if (memory != VK_NULL_HANDLE) vkFreeMemory(lDevice, memory, nullptr);
    }

    view = VK_NULL_HANDLE;
    image = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;

    format = VK_FORMAT_UNDEFINED;
    width = height = depth = 0;
    mipLevels = arrayLayers = 1;
    layout = ImageLayout::Undefined;
    ownership = Ownership::Owned;
}

ImageVK::ImageVK(ImageVK&& other) noexcept
    : lDevice(other.lDevice)
    , image(other.image)
    , memory(other.memory)
    , view(other.view)
    , format(other.format)
    , width(other.width)
    , height(other.height)
    , depth(other.depth)
    , mipLevels(other.mipLevels)
    , arrayLayers(other.arrayLayers)
    , layout(other.layout)
    , ownership(other.ownership) {
    
    // Reset other object
    other.lDevice = VK_NULL_HANDLE;
    other.image = VK_NULL_HANDLE;
    other.memory = VK_NULL_HANDLE;
    other.view = VK_NULL_HANDLE;
    other.format = VK_FORMAT_UNDEFINED;
    other.width = 0;
    other.height = 0;
    other.depth = 1;
    other.mipLevels = 1;
    other.arrayLayers = 1;
    other.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    other.ownership = Ownership::Owned;
}

ImageVK& ImageVK::operator=(ImageVK&& other) noexcept {
    if (this != &other) {
        cleanup();

        lDevice = other.lDevice;
        image = other.image;
        memory = other.memory;
        view = other.view;
        format = other.format;
        width = other.width;
        height = other.height;
        depth = other.depth;
        mipLevels = other.mipLevels;
        arrayLayers = other.arrayLayers;
        layout = other.layout;
        ownership = other.ownership;
        
        // Reset other object
        other.lDevice = VK_NULL_HANDLE;
        other.image = VK_NULL_HANDLE;
        other.memory = VK_NULL_HANDLE;
        other.view = VK_NULL_HANDLE;
        other.format = VK_FORMAT_UNDEFINED;
        other.width = 0;
        other.height = 0;
        other.depth = 1;
        other.mipLevels = 1;
        other.arrayLayers = 1;
        other.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        other.ownership = Ownership::Owned;
    }
    return *this;
}


ImageVK& ImageVK::createImage(const ImageConfig& config) {
    if (lDevice == VK_NULL_HANDLE || config.pDevice == VK_NULL_HANDLE) {
        std::cerr << "ImageVK: Cannot create image - device not set" << std::endl;
        return *this;
    }

    cleanup();

    // Store configuration
    width = config.width;
    height = config.height;
    depth = config.depth;
    mipLevels = config.mipLevels;
    arrayLayers = config.arrayLayers;
    format = config.format;
    layout = config.initialLayout;

    // Create image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = config.imageType;
    imageInfo.extent.width = config.width;
    imageInfo.extent.height = config.height;
    imageInfo.extent.depth = config.depth;
    imageInfo.mipLevels = config.mipLevels;
    imageInfo.arrayLayers = config.arrayLayers;
    imageInfo.format = config.format;
    imageInfo.tiling = config.tiling;
    imageInfo.initialLayout = config.initialLayout;
    imageInfo.usage = config.usage;
    imageInfo.samples = config.samples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(lDevice, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        std::cerr << "ImageVK: Failed to create image" << std::endl;
        return *this;
    }

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(lDevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = Device::findMemoryType(memRequirements.memoryTypeBits, config.memoryProperties, config.pDevice);

    if (vkAllocateMemory(lDevice, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        std::cerr << "ImageVK: Failed to allocate image memory" << std::endl;
        cleanup();
        return *this;
    }

    // Bind memory
    if (vkBindImageMemory(lDevice, image, memory, 0) != VK_SUCCESS) {
        std::cerr << "ImageVK: Failed to bind image memory" << std::endl;
        cleanup();
    }

    return *this;
}

ImageVK& ImageVK::createView(const ImageViewConfig& viewConfig) {
    if (image == VK_NULL_HANDLE) {
        std::cerr << "ImageVK: Cannot create image view - image not created" << std::endl;
        return *this;
    }

    // Clean up existing image view
    if (view != VK_NULL_HANDLE) {
        vkDestroyImageView(lDevice, view, nullptr);
        view = VK_NULL_HANDLE;
    }

    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image;
    createInfo.viewType = viewConfig.type;
    // Auto-select format if not specified
    createInfo.format = (viewConfig.format != VK_FORMAT_UNDEFINED) ? viewConfig.format : format;
    createInfo.components = viewConfig.components;
    createInfo.subresourceRange.aspectMask = viewConfig.aspectMask;
    createInfo.subresourceRange.baseMipLevel = viewConfig.baseMipLevel;
    createInfo.subresourceRange.levelCount = (viewConfig.mipLevels == VK_REMAINING_MIP_LEVELS) ? mipLevels : viewConfig.mipLevels;
    createInfo.subresourceRange.baseArrayLayer = viewConfig.baseArrayLayer;
    createInfo.subresourceRange.layerCount = (viewConfig.arrayLayers == VK_REMAINING_ARRAY_LAYERS) ? arrayLayers : viewConfig.arrayLayers;

    if (vkCreateImageView(lDevice, &createInfo, nullptr, &view) != VK_SUCCESS) {
        std::cerr << "ImageVK: Failed to create image view" << std::endl;
    }

    return *this;
}

ImageVK& ImageVK::setSwapchainImage(VkImage swapchainImage, VkFormat fmt, VkExtent2D extent) {
    cleanup();

    image = swapchainImage;
    format = fmt;
    width = extent.width;
    height = extent.height;
    depth = 1;
    mipLevels = 1;
    arrayLayers = 1;
    layout = ImageLayout::PresentSrcKHR;
    ownership = Ownership::External;

    return *this;
}

// Static helper functions
VkFormat ImageVK::getVulkanFormatFromChannels(int channels) {
    switch (channels) {
        case 1:  return VK_FORMAT_R8_UNORM;          // Grayscale
        case 2:  return VK_FORMAT_R8G8_UNORM;        // Grayscale + Alpha
        case 3:  return VK_FORMAT_R8G8B8A8_SRGB;     // Convert RGB to RGBA
        case 4:  return VK_FORMAT_R8G8B8A8_SRGB;     // RGBA
        default: return VK_FORMAT_R8G8B8A8_SRGB;
    }
}

std::vector<uint8_t> ImageVK::convertToValidData(int channels, int width, int height, const uint8_t* srcData) {
    if (channels == 3) {
        // Convert RGB to RGBA by adding alpha channel
        size_t pixelCount = width * height;
        std::vector<uint8_t> convertedData(pixelCount * 4);
        
        for (size_t i = 0; i < pixelCount; ++i) {
            convertedData[i * 4 + 0] = srcData[i * 3 + 0]; // R
            convertedData[i * 4 + 1] = srcData[i * 3 + 1]; // G
            convertedData[i * 4 + 2] = srcData[i * 3 + 2]; // B
            convertedData[i * 4 + 3] = 255;                // A (fully opaque)
        }
        
        return convertedData;
    } else {
        // For 1, 2, or 4 channels, use data as-is
        size_t dataSize = width * height * channels;
        return std::vector<uint8_t>(srcData, srcData + dataSize);
    }
}

uint32_t ImageVK::autoMipLevels(uint32_t width, uint32_t height) {
    return static_cast<uint32_t>(floor(log2(std::max(width, height)))) + 1;
}



SamplerConfig& SamplerConfig::withFilters(VkFilter magFilter, VkFilter minFilter) {
    this->magFilter = magFilter;
    this->minFilter = minFilter;
    return *this;
}

SamplerConfig& SamplerConfig::withMipmapMode(VkSamplerMipmapMode mode) {
    this->mipmapMode = mode;
    return *this;
}

SamplerConfig& SamplerConfig::withAddressModes(VkSamplerAddressMode mode) {
    withAddressModes(mode, mode, mode);
    return *this;
}

SamplerConfig& SamplerConfig::withAddressModes(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w) {
    this->addressModeU = u;
    this->addressModeV = v;
    this->addressModeW = w;
    return *this;
}

SamplerConfig& SamplerConfig::withAnisotropy(VkBool32 enable, float maxAniso) {
    this->anisotropyEnable = enable;
    this->maxAnisotropy = maxAniso;
    return *this;
}

SamplerConfig& SamplerConfig::withLodRange(float minLod, float maxLod, float bias) {
    this->minLod = minLod;
    this->maxLod = maxLod;
    this->mipLodBias = bias;
    return *this;
}

SamplerConfig& SamplerConfig::withBorderColor(VkBorderColor color) {
    this->borderColor = color;
    return *this;
}

SamplerConfig& SamplerConfig::withCompare(VkBool32 enable, VkCompareOp op) {
    this->compareEnable = enable;
    this->compareOp = op;
    return *this;
}

SamplerConfig& SamplerConfig::withPhysicalDevice(VkPhysicalDevice pDevice) {
    this->pDevice = pDevice;
    return *this;
}

SamplerVK::SamplerVK(VkDevice lDevice) {
    init(lDevice);
}
SamplerVK& SamplerVK::init(VkDevice lDevice) {
    this->lDevice = lDevice;
    return *this;
}
SamplerVK& SamplerVK::init(const Device* device) {
    lDevice = device ? device->lDevice : VK_NULL_HANDLE;
    return *this;
}

SamplerVK::SamplerVK(SamplerVK&& other) noexcept {
    lDevice = other.lDevice;
    sampler = other.sampler;

    other.lDevice = VK_NULL_HANDLE;
    other.sampler = VK_NULL_HANDLE;
}

SamplerVK& SamplerVK::operator=(SamplerVK&& other) noexcept {
    if (this != &other) {
        cleanup();

        lDevice = other.lDevice;
        sampler = other.sampler;
        
        other.lDevice = VK_NULL_HANDLE;
        other.sampler = VK_NULL_HANDLE;
    }
    return *this;
}

void SamplerVK::cleanup() {
    if (sampler != VK_NULL_HANDLE && lDevice != VK_NULL_HANDLE) {
        vkDestroySampler(lDevice, sampler, nullptr);
        sampler = VK_NULL_HANDLE;
    }
}


SamplerVK& SamplerVK::create(const SamplerConfig& config) {
    if (lDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("SamplerVK: Device not initialized");
    }

    cleanup(); // Clean up existing sampler if any

    VkSamplerCreateInfo createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.magFilter        = config.magFilter;
    createInfo.minFilter        = config.minFilter;
    createInfo.mipmapMode       = config.mipmapMode;
    createInfo.addressModeU     = config.addressModeU;
    createInfo.addressModeV     = config.addressModeV;
    createInfo.addressModeW     = config.addressModeW;
    createInfo.anisotropyEnable = config.anisotropyEnable;
    createInfo.mipLodBias       = config.mipLodBias;
    createInfo.minLod           = config.minLod;
    createInfo.maxLod           = config.maxLod;
    createInfo.borderColor      = config.borderColor;
    createInfo.compareEnable    = config.compareEnable;
    createInfo.compareOp        = config.compareOp;
    createInfo.maxAnisotropy    = getMaxAnisotropy(config.pDevice, config.maxAnisotropy);
    createInfo.unnormalizedCoordinates = config.unnormalizedCoordinates;

    VkResult result = vkCreateSampler(lDevice, &createInfo, nullptr, &sampler);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VkSampler");
    }

    return *this;
}

float SamplerVK::getMaxAnisotropy(VkPhysicalDevice pDevice, float requested) {
    if (pDevice == VK_NULL_HANDLE) {
        return 1.0f; // Safe fallback
    }

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(pDevice, &properties);

    return std::min(requested, properties.limits.maxSamplerAnisotropy);
}




TextureVK::TextureVK(VkDevice lDevice) {
    init(lDevice);
}
TextureVK& TextureVK::init(VkDevice lDevice) {
    image.init(lDevice);
    sampler.init(lDevice);
    return *this;
}
TextureVK& TextureVK::init(const Device* device) {
    if (device) {
        image.init(device);
        sampler.init(device);
    }
    return *this;
}


TextureVK::TextureVK(TextureVK&& other) noexcept
    : image(std::move(other.image))
    , sampler(std::move(other.sampler)) {}

TextureVK& TextureVK::operator=(TextureVK&& other) noexcept {
    if (this != &other) {
        image = std::move(other.image);
        sampler = std::move(other.sampler);
    }
    return *this;
}


TextureVK& TextureVK::createImage(const ImageConfig& config) {
    image.createImage(config);
    return *this;
}

TextureVK& TextureVK::createView(const ImageViewConfig& viewConfig) {
    image.createView(viewConfig);
    return *this;
}

TextureVK& TextureVK::createSampler(const SamplerConfig& config) {
    sampler.create(config);
    return *this;
}


void TextureVK::transitionLayout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout) {
    if (image.getImage() == VK_NULL_HANDLE) {
        std::cerr << "TextureVK: Cannot transition layout - image not created" << std::endl;
        return;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image.getImage();
    
    // Determine aspect mask based on format
    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        bool hasStencil =   image.getFormat() == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                            image.getFormat() == VK_FORMAT_D24_UNORM_S8_UINT;
        barrier.subresourceRange.aspectMask |= hasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;

    } else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = image.getMipLevels();
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = image.getArrayLayers();

    barrier.srcAccessMask = getAccessFlags(oldLayout);
    barrier.dstAccessMask = getAccessFlags(newLayout);

    VkPipelineStageFlags sourceStage = getStageFlags(oldLayout);
    VkPipelineStageFlags destinationStage = getStageFlags(newLayout);

    vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    image.setLayout(newLayout);
}

void TextureVK::copyFromBuffer(VkCommandBuffer cmd, VkBuffer srcBuffer) {
    if (image.getImage() == VK_NULL_HANDLE) {
        std::cerr << "TextureVK: Cannot copy from buffer - image not created" << std::endl;
        return;
    }

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = image.getExtent3D();

    vkCmdCopyBufferToImage(cmd, srcBuffer, image.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void TextureVK::generateMipmaps(VkCommandBuffer cmd, VkPhysicalDevice pDevice) {
    if (image.getImage() == VK_NULL_HANDLE) {
        std::cerr << "TextureVK: Cannot generate mipmaps - image not created" << std::endl;
        return;
    }

    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(pDevice, image.getFormat(), &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("TextureVK: texture image format does not support linear blitting!");
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image.getImage();
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = static_cast<int32_t>(image.getWidth());
    int32_t mipHeight = static_cast<int32_t>(image.getHeight());

    for (uint32_t i = 1; i < image.getMipLevels(); ++i) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(cmd, image.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            image.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = image.getMipLevels() - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    image.setLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}



TextureVK& TextureVK::transitionLayoutImmediate(VkCommandBuffer tempCmd, VkImageLayout oldLayout, VkImageLayout newLayout) {
    transitionLayout(tempCmd, oldLayout, newLayout);
    return *this;
}

TextureVK& TextureVK::copyFromBufferImmediate(VkCommandBuffer tempCmd, VkBuffer srcBuffer) {
    copyFromBuffer(tempCmd, srcBuffer);
    return *this;
}

TextureVK& TextureVK::generateMipmapsImmediate(VkCommandBuffer tempCmd, VkPhysicalDevice pDevice) {
    generateMipmaps(tempCmd, pDevice);
    return *this;
}


VkPipelineStageFlags TextureVK::getStageFlags(VkImageLayout layout) {
    switch (layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            return VK_PIPELINE_STAGE_HOST_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case VK_IMAGE_LAYOUT_GENERAL:
            return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        default:
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }
}

VkAccessFlags TextureVK::getAccessFlags(VkImageLayout layout) {
    switch (layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return 0;
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            return VK_ACCESS_HOST_WRITE_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_ACCESS_SHADER_READ_BIT;
        case VK_IMAGE_LAYOUT_GENERAL:
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        default:
            return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    }
}

