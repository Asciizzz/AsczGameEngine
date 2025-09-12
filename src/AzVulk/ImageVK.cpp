#include "AzVulk/ImageVK.hpp"
#include "AzVulk/Device.hpp"
#include <stdexcept>
#include <iostream>
#include <algorithm>

using namespace AzVulk;

// ImageConfig implementation
ImageConfig& ImageConfig::setDimensions(uint32_t w, uint32_t h, uint32_t d) {
    width = w;
    height = h;
    depth = d;
    return *this;
}

ImageConfig& ImageConfig::setFormat(VkFormat fmt) {
    format = fmt;
    return *this;
}

ImageConfig& ImageConfig::setUsage(VkImageUsageFlags usageFlags) {
    usage = usageFlags;
    return *this;
}

ImageConfig& ImageConfig::setMemoryProperties(VkMemoryPropertyFlags memProps) {
    memoryProperties = memProps;
    return *this;
}

ImageConfig& ImageConfig::setMipLevels(uint32_t levels) {
    mipLevels = levels;
    return *this;
}

ImageConfig& ImageConfig::setSamples(VkSampleCountFlagBits sampleCount) {
    samples = sampleCount;
    return *this;
}

ImageConfig& ImageConfig::setTiling(VkImageTiling imageTiling) {
    tiling = imageTiling;
    return *this;
}

ImageConfig ImageConfig::createDepthBuffer(uint32_t width, uint32_t height, VkFormat depthFormat) {
    ImageConfig config;
    config.setDimensions(width, height)
          .setFormat(depthFormat)
          .setUsage(ImageUsagePreset::DEPTH_BUFFER)
          .setMemoryProperties(MemoryPreset::DEVICE_LOCAL);
    return config;
}

ImageConfig ImageConfig::createTexture(uint32_t width, uint32_t height, VkFormat format, uint32_t mipLevels) {
    ImageConfig config;
    config.setDimensions(width, height)
          .setFormat(format)
          .setUsage(ImageUsagePreset::TEXTURE)
          .setMipLevels(mipLevels)
          .setMemoryProperties(MemoryPreset::DEVICE_LOCAL);
    return config;
}

ImageConfig ImageConfig::createRenderTarget(uint32_t width, uint32_t height, VkFormat format) {
    ImageConfig config;
    config.setDimensions(width, height)
          .setFormat(format)
          .setUsage(ImageUsagePreset::RENDER_TARGET)
          .setMemoryProperties(MemoryPreset::DEVICE_LOCAL);
    return config;
}

ImageConfig ImageConfig::createComputeStorage(uint32_t width, uint32_t height, VkFormat format) {
    ImageConfig config;
    config.setDimensions(width, height)
          .setFormat(format)
          .setUsage(ImageUsagePreset::COMPUTE_STORAGE)
          .setMemoryProperties(MemoryPreset::DEVICE_LOCAL);
    return config;
}

ImageConfig ImageConfig::createPostProcessBuffer(uint32_t width, uint32_t height) {
    ImageConfig config;
    config.setDimensions(width, height)
          .setFormat(VK_FORMAT_R8G8B8A8_UNORM)
          .setUsage(ImageUsagePreset::POST_PROCESS)
          .setMemoryProperties(MemoryPreset::DEVICE_LOCAL);
    return config;
}

// ImageViewConfig implementation
ImageViewConfig ImageViewConfig::createDefault(VkImageAspectFlags aspect) {
    ImageViewConfig config;
    config.aspectMask = aspect;
    return config;
}

ImageViewConfig ImageViewConfig::createDepthView() {
    ImageViewConfig config;
    config.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    return config;
}

ImageViewConfig ImageViewConfig::createColorView(uint32_t mipLevels) {
    ImageViewConfig config;
    config.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    config.mipLevels = mipLevels;
    return config;
}

ImageViewConfig ImageViewConfig::createCubeMapView() {
    ImageViewConfig config;
    config.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    config.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    config.arrayLayers = 6;
    return config;
}

// ImageVK implementation
ImageVK::ImageVK(const Device* device) : device(device) {
}

ImageVK::~ImageVK() {
    cleanup();
}

ImageVK::ImageVK(ImageVK&& other) noexcept
    : device(other.device)
    , image(other.image)
    , memory(other.memory)
    , imageView(other.imageView)
    , format(other.format)
    , width(other.width)
    , height(other.height)
    , depth(other.depth)
    , mipLevels(other.mipLevels)
    , arrayLayers(other.arrayLayers)
    , currentLayout(other.currentLayout)
    , debugName(std::move(other.debugName)) {
    
    // Reset other object
    other.device = nullptr;
    other.image = VK_NULL_HANDLE;
    other.memory = VK_NULL_HANDLE;
    other.imageView = VK_NULL_HANDLE;
    other.format = VK_FORMAT_UNDEFINED;
    other.width = 0;
    other.height = 0;
    other.depth = 1;
    other.mipLevels = 1;
    other.arrayLayers = 1;
    other.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

ImageVK& ImageVK::operator=(ImageVK&& other) noexcept {
    if (this != &other) {
        cleanup();
        
        device = other.device;
        image = other.image;
        memory = other.memory;
        imageView = other.imageView;
        format = other.format;
        width = other.width;
        height = other.height;
        depth = other.depth;
        mipLevels = other.mipLevels;
        arrayLayers = other.arrayLayers;
        currentLayout = other.currentLayout;
        debugName = std::move(other.debugName);
        
        // Reset other object
        other.device = nullptr;
        other.image = VK_NULL_HANDLE;
        other.memory = VK_NULL_HANDLE;
        other.imageView = VK_NULL_HANDLE;
        other.format = VK_FORMAT_UNDEFINED;
        other.width = 0;
        other.height = 0;
        other.depth = 1;
        other.mipLevels = 1;
        other.arrayLayers = 1;
        other.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    return *this;
}

bool ImageVK::create(const ImageConfig& config) {
    if (!device) {
        std::cerr << "ImageVK: Device is null" << std::endl;
        return false;
    }

    cleanup(); // Clean up any existing resources

    // Store configuration
    width = config.width;
    height = config.height;
    depth = config.depth;
    mipLevels = config.mipLevels;
    arrayLayers = config.arrayLayers;
    format = config.format;
    currentLayout = config.initialLayout;

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

    if (vkCreateImage(device->lDevice, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        std::cerr << "ImageVK: Failed to create image" << std::endl;
        return false;
    }

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device->lDevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, config.memoryProperties);

    if (vkAllocateMemory(device->lDevice, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        std::cerr << "ImageVK: Failed to allocate image memory" << std::endl;
        cleanup();
        return false;
    }

    // Bind memory
    if (vkBindImageMemory(device->lDevice, image, memory, 0) != VK_SUCCESS) {
        std::cerr << "ImageVK: Failed to bind image memory" << std::endl;
        cleanup();
        return false;
    }

    return true;
}

bool ImageVK::createImageView(const ImageViewConfig& viewConfig) {
    if (image == VK_NULL_HANDLE) {
        std::cerr << "ImageVK: Cannot create image view - image not created" << std::endl;
        return false;
    }

    // Clean up existing image view
    if (imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device->lDevice, imageView, nullptr);
        imageView = VK_NULL_HANDLE;
    }

    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image;
    createInfo.viewType = viewConfig.viewType;
    createInfo.format = (viewConfig.format != VK_FORMAT_UNDEFINED) ? viewConfig.format : format;
    createInfo.components = viewConfig.components;
    createInfo.subresourceRange.aspectMask = viewConfig.aspectMask;
    createInfo.subresourceRange.baseMipLevel = viewConfig.baseMipLevel;
    createInfo.subresourceRange.levelCount = (viewConfig.mipLevels == VK_REMAINING_MIP_LEVELS) ? mipLevels : viewConfig.mipLevels;
    createInfo.subresourceRange.baseArrayLayer = viewConfig.baseArrayLayer;
    createInfo.subresourceRange.layerCount = (viewConfig.arrayLayers == VK_REMAINING_ARRAY_LAYERS) ? arrayLayers : viewConfig.arrayLayers;

    if (vkCreateImageView(device->lDevice, &createInfo, nullptr, &imageView) != VK_SUCCESS) {
        std::cerr << "ImageVK: Failed to create image view" << std::endl;
        return false;
    }

    return true;
}

bool ImageVK::createDepthBuffer(uint32_t width, uint32_t height, VkFormat depthFormat) {
    ImageConfig config = ImageConfig::createDepthBuffer(width, height, depthFormat);
    if (!create(config)) return false;

    ImageViewConfig viewConfig = ImageViewConfig::createDepthView();
    return createImageView(viewConfig);
}

bool ImageVK::createTexture(uint32_t width, uint32_t height, VkFormat format, uint32_t mipLevels) {
    ImageConfig config = ImageConfig::createTexture(width, height, format, mipLevels);
    if (!create(config)) return false;

    ImageViewConfig viewConfig = ImageViewConfig::createColorView(mipLevels);
    return createImageView(viewConfig);
}

bool ImageVK::createTexture(uint32_t width, uint32_t height, int channels, const uint8_t* data, VkBuffer stagingBuffer) {
    // Get appropriate Vulkan format and convert data
    VkFormat textureFormat = ImageVK::getVulkanFormatFromChannels(channels);
    std::vector<uint8_t> vulkanData = ImageVK::convertTextureDataForVulkan(channels, width, height, data);
    
    // Dynamic mipmap levels
    uint32_t mipLevels = static_cast<uint32_t>(floor(log2(std::max(width, height)))) + 1;
    
    // Create the texture
    if (!createTexture(width, height, textureFormat, mipLevels)) {
        return false;
    }
    
    // Upload texture data using fluent interface
    transitionLayoutImmediate(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        .copyFromBufferImmediate(stagingBuffer, width, height)
        .generateMipmapsImmediate();
        
    return true;
}

bool ImageVK::createRenderTarget(uint32_t width, uint32_t height, VkFormat format) {
    ImageConfig config = ImageConfig::createRenderTarget(width, height, format);
    if (!create(config)) return false;

    ImageViewConfig viewConfig = ImageViewConfig::createColorView();
    return createImageView(viewConfig);
}

bool ImageVK::createComputeStorage(uint32_t width, uint32_t height, VkFormat format) {
    ImageConfig config = ImageConfig::createComputeStorage(width, height, format);
    if (!create(config)) return false;

    ImageViewConfig viewConfig = ImageViewConfig::createColorView();
    return createImageView(viewConfig);
}

bool ImageVK::createPostProcessBuffer(uint32_t width, uint32_t height) {
    ImageConfig config = ImageConfig::createPostProcessBuffer(width, height);
    if (!create(config)) return false;

    ImageViewConfig viewConfig = ImageViewConfig::createColorView();
    return createImageView(viewConfig);
}

void ImageVK::transitionLayout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout,
                                  uint32_t baseMipLevel, uint32_t mipLevels, uint32_t baseArrayLayer, uint32_t arrayLayers) {
    if (image == VK_NULL_HANDLE) {
        std::cerr << "ImageVK: Cannot transition layout - image not created" << std::endl;
        return;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    
    // Determine aspect mask based on format
    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (hasStencilComponent(format)) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    } else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    
    barrier.subresourceRange.baseMipLevel = baseMipLevel;
    barrier.subresourceRange.levelCount = (mipLevels == VK_REMAINING_MIP_LEVELS) ? this->mipLevels - baseMipLevel : mipLevels;
    barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
    barrier.subresourceRange.layerCount = (arrayLayers == VK_REMAINING_ARRAY_LAYERS) ? this->arrayLayers - baseArrayLayer : arrayLayers;

    barrier.srcAccessMask = getAccessFlags(oldLayout);
    barrier.dstAccessMask = getAccessFlags(newLayout);

    VkPipelineStageFlags sourceStage = getStageFlags(oldLayout);
    VkPipelineStageFlags destinationStage = getStageFlags(newLayout);

    vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    currentLayout = newLayout;
}

ImageVK& ImageVK::transitionLayoutImmediate(VkImageLayout oldLayout, VkImageLayout newLayout) {
    // Create temporary command buffer and execute immediately
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = device->graphicsPoolWrapper.pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device->lDevice, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    transitionLayout(commandBuffer, oldLayout, newLayout);
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(device->graphicsQueue);

    vkFreeCommandBuffers(device->lDevice, device->graphicsPoolWrapper.pool, 1, &commandBuffer);
    return *this;
}

void ImageVK::copyFromBuffer(VkCommandBuffer cmd, VkBuffer srcBuffer, 
                                uint32_t width, uint32_t height, uint32_t mipLevel) {
    if (image == VK_NULL_HANDLE) {
        std::cerr << "ImageVK: Cannot copy from buffer - image not created" << std::endl;
        return;
    }

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = mipLevel;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(cmd, srcBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

ImageVK& ImageVK::copyFromBufferImmediate(VkBuffer srcBuffer, uint32_t width, uint32_t height, uint32_t mipLevel) {
    // Create temporary command buffer and execute immediately
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = device->graphicsPoolWrapper.pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device->lDevice, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    copyFromBuffer(commandBuffer, srcBuffer, width, height, mipLevel);
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(device->graphicsQueue);

    vkFreeCommandBuffers(device->lDevice, device->graphicsPoolWrapper.pool, 1, &commandBuffer);
    return *this;
}

void ImageVK::generateMipmaps(VkCommandBuffer cmd) {
    if (image == VK_NULL_HANDLE) {
        std::cerr << "ImageVK: Cannot generate mipmaps - image not created" << std::endl;
        return;
    }

    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(device->pDevice, format, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("ImageVK: texture image format does not support linear blitting!");
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = static_cast<int32_t>(width);
    int32_t mipHeight = static_cast<int32_t>(height);

    for (uint32_t i = 1; i < mipLevels; ++i) {
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

        vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

ImageVK& ImageVK::generateMipmapsImmediate() {
    // Create temporary command buffer and execute immediately
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = device->graphicsPoolWrapper.pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device->lDevice, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    generateMipmaps(commandBuffer);
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(device->graphicsQueue);

    vkFreeCommandBuffers(device->lDevice, device->graphicsPoolWrapper.pool, 1, &commandBuffer);
    return *this;
}

bool ImageVK::isValid() const {
    return image != VK_NULL_HANDLE && memory != VK_NULL_HANDLE;
}

void ImageVK::cleanup() {
    if (!device) return;

    VkDevice lDevice = device->lDevice;

    if (imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(lDevice, imageView, nullptr);
        imageView = VK_NULL_HANDLE;
    }

    if (image != VK_NULL_HANDLE) {
        vkDestroyImage(lDevice, image, nullptr);
        image = VK_NULL_HANDLE;
    }

    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(lDevice, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }

    format = VK_FORMAT_UNDEFINED;
    width = height = depth = 0;
    mipLevels = arrayLayers = 1;
    currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void ImageVK::setDebugName(const std::string& name) {
    debugName = name;
    
    // Set debug name in Vulkan if debug utils are available
    if (device && image != VK_NULL_HANDLE) {
        VkDebugUtilsObjectNameInfoEXT nameInfo{};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
        nameInfo.objectHandle = reinterpret_cast<uint64_t>(image);
        nameInfo.pObjectName = debugName.c_str();
        
        // Note: This requires the debug utils extension to be enabled
        // You might want to add a check for this extension
    }
}

uint32_t ImageVK::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    return device->findMemoryType(typeFilter, properties);
}

bool ImageVK::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

// Static helper functions
VkFormat ImageVK::getVulkanFormatFromChannels(int channels) {
    switch (channels) {
        case 1: return VK_FORMAT_R8_UNORM;          // Grayscale
        case 2: return VK_FORMAT_R8G8_UNORM;        // Grayscale + Alpha
        case 3: return VK_FORMAT_R8G8B8A8_SRGB;     // Convert RGB to RGBA
        case 4: return VK_FORMAT_R8G8B8A8_SRGB;     // RGBA
        default: 
            std::cerr << "Warning: Unsupported channel count " << channels << ", defaulting to RGBA" << std::endl;
            return VK_FORMAT_R8G8B8A8_SRGB;
    }
}

std::vector<uint8_t> ImageVK::convertTextureDataForVulkan(int channels, int width, int height, const uint8_t* srcData) {
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


VkPipelineStageFlags ImageVK::getStageFlags(VkImageLayout layout) {
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

VkAccessFlags ImageVK::getAccessFlags(VkImageLayout layout) {
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

// TemporaryImage implementation
TemporaryImage::TemporaryImage(const Device* device, const ImageConfig& config) : image(device) {
    if (!image.create(config)) {
        throw std::runtime_error("Failed to create temporary image");
    }
}

TemporaryImage::~TemporaryImage() {
    // Automatic cleanup via ImageVK destructor
}

// Factory functions
namespace AzVulk::ImageFactory {
    UniquePtr<ImageVK> createDepthBuffer(const Device* device, uint32_t width, uint32_t height, VkFormat depthFormat) {
        auto image = MakeUnique<ImageVK>(device);
        if (!image->createDepthBuffer(width, height, depthFormat)) {
            return nullptr;
        }
        return image;
    }

    UniquePtr<ImageVK> createTexture(const Device* device, uint32_t width, uint32_t height, VkFormat format, uint32_t mipLevels) {
        auto image = MakeUnique<ImageVK>(device);
        if (!image->createTexture(width, height, format, mipLevels)) {
            return nullptr;
        }
        return image;
    }

    UniquePtr<ImageVK> createRenderTarget(const Device* device, uint32_t width, uint32_t height, VkFormat format) {
        auto image = MakeUnique<ImageVK>(device);
        if (!image->createRenderTarget(width, height, format)) {
            return nullptr;
        }
        return image;
    }

    UniquePtr<ImageVK> createComputeStorage(const Device* device, uint32_t width, uint32_t height, VkFormat format) {
        auto image = MakeUnique<ImageVK>(device);
        if (!image->createComputeStorage(width, height, format)) {
            return nullptr;
        }
        return image;
    }

    UniquePtr<ImageVK> createPostProcessBuffer(const Device* device, uint32_t width, uint32_t height) {
        auto image = MakeUnique<ImageVK>(device);
        if (!image->createPostProcessBuffer(width, height)) {
            return nullptr;
        }
        return image;
    }
}

namespace AzVulk {

// Static utility functions for backward compatibility
void createImage(const Device* device, uint32_t width, uint32_t height, VkFormat format, 
                VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = format;
    imageInfo.tiling        = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = usage;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device->lDevice, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device->lDevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = device->findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device->lDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(device->lDevice, image, imageMemory, 0);
}

VkImageView createImageView(const Device* device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = format;
    viewInfo.subresourceRange.aspectMask     = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    VkImageView imageView;
    if (vkCreateImageView(device->lDevice, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image view!");
    }

    return imageView;
}

}