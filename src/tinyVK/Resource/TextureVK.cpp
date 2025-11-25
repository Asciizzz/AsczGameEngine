#include "tinyVk/Resource/TextureVk.hpp"
#include "tinyVk/System/CmdBuffer.hpp"

#include <stdexcept>
#include <iostream>
#include <algorithm>

using namespace tinyVk;

void ImageVk::cleanup() {
    if (device_ == VK_NULL_HANDLE) return;

    if (view_ != VK_NULL_HANDLE) vkDestroyImageView(device_, view_, nullptr);

    if (ownership_ == TOwnership::Owned) {
        if (image_ != VK_NULL_HANDLE)  vkDestroyImage(device_, image_, nullptr);
        if (memory_ != VK_NULL_HANDLE) vkFreeMemory(device_, memory_, nullptr);
    }

    view_ = VK_NULL_HANDLE;
    image_ = VK_NULL_HANDLE;
    memory_ = VK_NULL_HANDLE;

    format_ = VK_FORMAT_UNDEFINED;
    width_ = height_ = depth_ = 0;
    mipLevels_ = arrayLayers_ = 1;
    layout_ = ImageLayout::Undefined;
    ownership_ = TOwnership::Owned;
}

ImageVk::ImageVk(ImageVk&& other) noexcept
    : device_(other.device_)
    , image_(other.image_)
    , memory_(other.memory_)
    , view_(other.view_)
    , format_(other.format_)
    , width_(other.width_)
    , height_(other.height_)
    , depth_(other.depth_)
    , mipLevels_(other.mipLevels_)
    , arrayLayers_(other.arrayLayers_)
    , layout_(other.layout_)
    , ownership_(other.ownership_) {
    
    // Reset other object
    other.device_ = VK_NULL_HANDLE;
    other.image_ = VK_NULL_HANDLE;
    other.memory_ = VK_NULL_HANDLE;
    other.view_ = VK_NULL_HANDLE;
    other.format_ = VK_FORMAT_UNDEFINED;
    other.width_ = 0;
    other.height_ = 0;
    other.depth_ = 1;
    other.mipLevels_ = 1;
    other.arrayLayers_ = 1;
    other.layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    other.ownership_ = TOwnership::Owned;
}

ImageVk& ImageVk::operator=(ImageVk&& other) noexcept {
    if (this != &other) {
        cleanup();

        device_ = other.device_;
        image_ = other.image_;
        memory_ = other.memory_;
        view_ = other.view_;
        format_ = other.format_;
        width_ = other.width_;
        height_ = other.height_;
        depth_ = other.depth_;
        mipLevels_ = other.mipLevels_;
        arrayLayers_ = other.arrayLayers_;
        layout_ = other.layout_;
        ownership_ = other.ownership_;

        other.device_ = VK_NULL_HANDLE;
        other.image_ = VK_NULL_HANDLE;
        other.memory_ = VK_NULL_HANDLE;
        other.view_ = VK_NULL_HANDLE;
        other.format_ = VK_FORMAT_UNDEFINED;
        other.width_ = 0;
        other.height_ = 0;
        other.depth_ = 1;
        other.mipLevels_ = 1;
        other.arrayLayers_ = 1;
        other.layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
        other.ownership_ = TOwnership::Owned;
    }
    return *this;
}


ImageVk& ImageVk::createImage(const ImageConfig& config) {
    if (config.device == VK_NULL_HANDLE || config.pDevice == VK_NULL_HANDLE) {
        std::cerr << "ImageVk: Cannot create image - devices not set" << std::endl;
        return *this;
    }

    device_ = config.device;

    cleanup();

    // Store configuration
    width_       = config.width;
    height_      = config.height;
    depth_       = config.depth;
    mipLevels_   = config.mipLevels;
    arrayLayers_ = config.arrayLayers;
    format_      = config.format;
    layout_      = config.initialLayout;

    // Create image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = config.imageType;
    imageInfo.extent.width  = config.width;
    imageInfo.extent.height = config.height;
    imageInfo.extent.depth  = config.depth;
    imageInfo.mipLevels     = config.mipLevels;
    imageInfo.arrayLayers   = config.arrayLayers;
    imageInfo.format        = config.format;
    imageInfo.tiling        = config.tiling;
    imageInfo.initialLayout = config.initialLayout;
    imageInfo.usage         = config.usage;
    imageInfo.samples       = config.samples;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device_, &imageInfo, nullptr, &image_) != VK_SUCCESS) {
        std::cerr << "ImageVk: Failed to create image" << std::endl;
        return *this;
    }

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device_, image_, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = Device::findMemoryType(memRequirements.memoryTypeBits, config.memProps, config.pDevice);

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory_) != VK_SUCCESS) {
        std::cerr << "ImageVk: Failed to allocate image memory" << std::endl;
        cleanup();
        return *this;
    }

    // Bind memory
    if (vkBindImageMemory(device_, image_, memory_, 0) != VK_SUCCESS) {
        std::cerr << "ImageVk: Failed to bind image memory" << std::endl;
        cleanup();
    }

    return *this;
}

ImageVk& ImageVk::createView(const ImageViewConfig& viewConfig) {
    if (image_ == VK_NULL_HANDLE) {
        std::cerr << "ImageVk: Cannot create image view - image not created" << std::endl;
        return *this;
    }

    // Clean up existing image view
    if (view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, view_, nullptr);
        view_ = VK_NULL_HANDLE;
    }

    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image_;
    createInfo.viewType = viewConfig.type;
    // Auto-select format if not specified
    createInfo.format = (viewConfig.format != VK_FORMAT_UNDEFINED) ? viewConfig.format : format_;
    createInfo.components = viewConfig.components;
    createInfo.subresourceRange.aspectMask = viewConfig.aspectMask;
    createInfo.subresourceRange.baseMipLevel = viewConfig.baseMipLevel;
    createInfo.subresourceRange.levelCount = (viewConfig.mipLevels == VK_REMAINING_MIP_LEVELS) ? mipLevels_ : viewConfig.mipLevels;
    createInfo.subresourceRange.baseArrayLayer = viewConfig.baseArrayLayer;
    createInfo.subresourceRange.layerCount = (viewConfig.arrayLayers == VK_REMAINING_ARRAY_LAYERS) ? arrayLayers_ : viewConfig.arrayLayers;

    if (vkCreateImageView(device_, &createInfo, nullptr, &view_) != VK_SUCCESS) {
        std::cerr << "ImageVk: Failed to create image view" << std::endl;
    }

    return *this;
}

ImageVk& ImageVk::wrapExternalImage(VkDevice device, VkImage extImage, VkFormat fmt, VkExtent2D extent) {
    cleanup();

    device_      = device;
    image_       = extImage;
    format_      = fmt;
    width_       = extent.width;
    height_      = extent.height;
    depth_       = 1;
    mipLevels_   = 1;
    arrayLayers_ = 1;
    layout_      = ImageLayout::PresentSrcKHR;
    ownership_   = TOwnership::External;

    return *this;
}

uint32_t ImageVk::autoMipLevels(uint32_t width, uint32_t height) {
    return static_cast<uint32_t>(floor(log2(std::max(width, height)))) + 1;
}


SamplerVk::SamplerVk(SamplerVk&& other) noexcept {
    device_    = other.device_;
    sampler_   = other.sampler_;

    other.device_    = VK_NULL_HANDLE;
    other.sampler_   = VK_NULL_HANDLE;
}

SamplerVk& SamplerVk::operator=(SamplerVk&& other) noexcept {
    if (this != &other) {
        cleanup();

        device_    = other.device_;
        sampler_   = other.sampler_;

        other.device_    = VK_NULL_HANDLE;
        other.sampler_   = VK_NULL_HANDLE;
    }
    return *this;
}

void SamplerVk::cleanup() {
    if (sampler_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, sampler_, nullptr);
        sampler_ = VK_NULL_HANDLE;
    }
}

void SamplerVk::create(const SamplerConfig& config) {
    if (config.device == VK_NULL_HANDLE) return;

    device_ = config.device;

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

    VkResult result = vkCreateSampler(device_, &createInfo, nullptr, &sampler_);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VkSampler");
    }
}

float SamplerVk::getMaxAnisotropy(VkPhysicalDevice pDevice, float requested) {
    if (pDevice == VK_NULL_HANDLE) {
        return 1.0f; // Safe fallback
    }

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(pDevice, &properties);

    return std::min(requested, properties.limits.maxSamplerAnisotropy);
}


void TextureVk::transitionLayout(ImageVk& imageVk, VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout) {
    if (imageVk.image() == VK_NULL_HANDLE) {
        std::cerr << "TextureVk: Cannot transition layout - image not created" << std::endl;
        return;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = imageVk.image();
    
    // Determine aspect mask based on format
    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        bool hasStencil =   imageVk.format() == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                            imageVk.format() == VK_FORMAT_D24_UNORM_S8_UINT;
        barrier.subresourceRange.aspectMask |= hasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;

    } else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = imageVk.mipLevels();
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = imageVk.arrayLayers();

    barrier.srcAccessMask = getAccessFlags(oldLayout);
    barrier.dstAccessMask = getAccessFlags(newLayout);

    VkPipelineStageFlags sourceStage = getStageFlags(oldLayout);
    VkPipelineStageFlags destinationStage = getStageFlags(newLayout);

    vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    imageVk.setLayout(newLayout);
}

void TextureVk::copyFromBuffer(ImageVk& imageVk, VkCommandBuffer cmd, VkBuffer srcBuffer) {
    if (imageVk.image() == VK_NULL_HANDLE) {
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
    region.imageExtent = imageVk.extent3D();

    vkCmdCopyBufferToImage(cmd, srcBuffer, imageVk.image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void TextureVk::generateMipmaps(ImageVk& imageVk    , VkCommandBuffer cmd, VkPhysicalDevice pDevice) {
    if (imageVk.image() == VK_NULL_HANDLE) {
        std::cerr << "TextureVk: Cannot generate mipmaps - image not created" << std::endl;
        return;
    }

    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(pDevice, imageVk.format(), &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("TextureVk: texture image format does not support linear blitting!");
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = imageVk.image();
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = static_cast<int32_t>(imageVk.width());
    int32_t mipHeight = static_cast<int32_t>(imageVk.height());

    for (uint32_t i = 1; i < imageVk.mipLevels(); ++i) {
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

        vkCmdBlitImage(cmd, imageVk.image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            imageVk.image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = imageVk.mipLevels() - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    imageVk.setLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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

