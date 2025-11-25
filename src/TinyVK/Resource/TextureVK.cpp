#include "tinyVk/Resource/TextureVk.hpp"
#include "tinyVk/System/CmdBuffer.hpp"

#include <stdexcept>
#include <iostream>
#include <algorithm>

using namespace tinyVk;

// ImageConfig implementation
ImageConfig& ImageConfig::withDimensions(uint32_t w, uint32_t h, uint32_t d) {
    width = w;
    height = h;
    depth = d;
    return *this;
}

ImageConfig& ImageConfig::withAutoMipLevels() {
    mipLevels = ImageVk::autoMipLevels(width, height);
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
    mipLevels = ImageVk::autoMipLevels(width, height);
    return *this;
}



void ImageVk::cleanup() {
    if (device == VK_NULL_HANDLE) return;

    if (view != VK_NULL_HANDLE) vkDestroyImageView(device, view, nullptr);

    if (ownership == TOwnership::Owned) {
        if (image != VK_NULL_HANDLE)  vkDestroyImage(device, image, nullptr);
        if (memory != VK_NULL_HANDLE) vkFreeMemory(device, memory, nullptr);
    }

    view = VK_NULL_HANDLE;
    image = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;

    format = VK_FORMAT_UNDEFINED;
    width = height = depth = 0;
    mipLevels = arrayLayers = 1;
    layout = ImageLayout::Undefined;
    ownership = TOwnership::Owned;
}

ImageVk::ImageVk(ImageVk&& other) noexcept
    : device(other.device)
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
    other.device = VK_NULL_HANDLE;
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
    other.ownership = TOwnership::Owned;
}

ImageVk& ImageVk::operator=(ImageVk&& other) noexcept {
    if (this != &other) {
        cleanup();

        device = other.device;
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
        other.device = VK_NULL_HANDLE;
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
        other.ownership = TOwnership::Owned;
    }
    return *this;
}


ImageVk& ImageVk::createImage(const ImageConfig& config) {
    if (device == VK_NULL_HANDLE || config.pDevice == VK_NULL_HANDLE) {
        std::cerr << "ImageVk: Cannot create image - device not set" << std::endl;
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

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        std::cerr << "ImageVk: Failed to create image" << std::endl;
        return *this;
    }

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = Device::findMemoryType(memRequirements.memoryTypeBits, config.memoryProperties, config.pDevice);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        std::cerr << "ImageVk: Failed to allocate image memory" << std::endl;
        cleanup();
        return *this;
    }

    // Bind memory
    if (vkBindImageMemory(device, image, memory, 0) != VK_SUCCESS) {
        std::cerr << "ImageVk: Failed to bind image memory" << std::endl;
        cleanup();
    }

    return *this;
}

ImageVk& ImageVk::createView(const ImageViewConfig& viewConfig) {
    if (image == VK_NULL_HANDLE) {
        std::cerr << "ImageVk: Cannot create image view - image not created" << std::endl;
        return *this;
    }

    // Clean up existing image view
    if (view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, view, nullptr);
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

    if (vkCreateImageView(device, &createInfo, nullptr, &view) != VK_SUCCESS) {
        std::cerr << "ImageVk: Failed to create image view" << std::endl;
    }

    return *this;
}

ImageVk& ImageVk::wrapExternalImage(VkImage extImage, VkFormat fmt, VkExtent2D extent) {
    cleanup();

    image = extImage;
    format = fmt;
    width = extent.width;
    height = extent.height;
    depth = 1;
    mipLevels = 1;
    arrayLayers = 1;
    layout = ImageLayout::PresentSrcKHR;
    ownership = TOwnership::External;

    return *this;
}

uint32_t ImageVk::autoMipLevels(uint32_t width, uint32_t height) {
    return static_cast<uint32_t>(floor(log2(std::max(width, height)))) + 1;
}


SamplerVk::SamplerVk(SamplerVk&& other) noexcept {
    device = other.device;
    sampler = other.sampler;
    ownership = other.ownership;

    other.device = VK_NULL_HANDLE;
    other.sampler = VK_NULL_HANDLE;
    other.ownership = TOwnership::Owned;
}

SamplerVk& SamplerVk::operator=(SamplerVk&& other) noexcept {
    if (this != &other) {
        cleanup();

        device = other.device;
        sampler = other.sampler;
        ownership = other.ownership;

        other.device = VK_NULL_HANDLE;
        other.sampler = VK_NULL_HANDLE;
        other.ownership = TOwnership::Owned;
    }
    return *this;
}

void SamplerVk::cleanup() {
    if (sampler != VK_NULL_HANDLE && device != VK_NULL_HANDLE && ownership == TOwnership::Owned) {
        vkDestroySampler(device, sampler, nullptr);
        sampler = VK_NULL_HANDLE;

        ownership = TOwnership::Owned;
    }
}


SamplerVk& SamplerVk::create(const SamplerConfig& config) {
    if (device == VK_NULL_HANDLE) {
        throw std::runtime_error("SamplerVk: Device not initialized");
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

    VkResult result = vkCreateSampler(device, &createInfo, nullptr, &sampler);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VkSampler");
    }

    ownership = TOwnership::Owned;

    return *this;
}

SamplerVk& SamplerVk::set(VkSampler sampler) {
    cleanup();
    this->sampler = sampler;
    ownership = TOwnership::External;
    return *this;
}

float SamplerVk::getMaxAnisotropy(VkPhysicalDevice pDevice, float requested) {
    if (pDevice == VK_NULL_HANDLE) {
        return 1.0f; // Safe fallback
    }

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(pDevice, &properties);

    return std::min(requested, properties.limits.maxSamplerAnisotropy);
}



TextureVk::TextureVk(TextureVk&& other) noexcept
    : image(std::move(other.image))
    , sampler(std::move(other.sampler)) {}

TextureVk& TextureVk::operator=(TextureVk&& other) noexcept {
    if (this != &other) {
        image = std::move(other.image);
        sampler = std::move(other.sampler);
    }
    return *this;
}


TextureVk& TextureVk::createImage(const ImageConfig& config) {
    image.createImage(config);
    return *this;
}

TextureVk& TextureVk::createView(const ImageViewConfig& viewConfig) {
    image.createView(viewConfig);
    return *this;
}

TextureVk& TextureVk::createSampler(const SamplerConfig& config) {
    sampler.create(config);
    return *this;
}

TextureVk& TextureVk::setSampler(VkSampler sampler) {
    this->sampler.set(sampler);
    return *this;
}


void TextureVk::transitionLayout(ImageVk& image, VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout) {
    if (image.getImage() == VK_NULL_HANDLE) {
        std::cerr << "TextureVk: Cannot transition layout - image not created" << std::endl;
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

void TextureVk::copyFromBuffer(ImageVk& image, VkCommandBuffer cmd, VkBuffer srcBuffer) {
    if (image.getImage() == VK_NULL_HANDLE) {
        std::cerr << "TextureVk: Cannot copy from buffer - image not created" << std::endl;
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

void TextureVk::generateMipmaps(ImageVk& image, VkCommandBuffer cmd, VkPhysicalDevice pDevice) {
    if (image.getImage() == VK_NULL_HANDLE) {
        std::cerr << "TextureVk: Cannot generate mipmaps - image not created" << std::endl;
        return;
    }

    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(pDevice, image.getFormat(), &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("TextureVk: texture image format does not support linear blitting!");
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

VkPipelineStageFlags TextureVk::getStageFlags(VkImageLayout layout) {
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

VkAccessFlags TextureVk::getAccessFlags(VkImageLayout layout) {
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

