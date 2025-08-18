// DepthManager.cpp
#include "AzVulk/DepthManager.hpp"
#include "AzVulk/Device.hpp"

#include <stdexcept>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace AzVulk {

    DepthManager::DepthManager(const Device& device)
        : vulkanDevice(device),
          depthSampler(VK_NULL_HANDLE),
          depthSamplerView(VK_NULL_HANDLE),
          depthImage(VK_NULL_HANDLE),
          depthImageMemory(VK_NULL_HANDLE),
          depthImageView(VK_NULL_HANDLE),
          depthSampleImage(VK_NULL_HANDLE),
          depthSampleImageMemory(VK_NULL_HANDLE),
          msaaSamples(VK_SAMPLE_COUNT_1_BIT),
          depthResolveSupported(false)
    {}

    DepthManager::~DepthManager() {
        cleanup();
    }

    void DepthManager::cleanup() {
        VkDevice logicalDevice = vulkanDevice.device;

        if (depthSampler != VK_NULL_HANDLE) {
            vkDestroySampler(logicalDevice, depthSampler, nullptr);
            depthSampler = VK_NULL_HANDLE;
        }

        if (depthSamplerView != VK_NULL_HANDLE) {
            vkDestroyImageView(logicalDevice, depthSamplerView, nullptr);
            depthSamplerView = VK_NULL_HANDLE;
        }

        if (depthSampleImage != VK_NULL_HANDLE) {
            vkDestroyImage(logicalDevice, depthSampleImage, nullptr);
            depthSampleImage = VK_NULL_HANDLE;
        }

        if (depthSampleImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(logicalDevice, depthSampleImageMemory, nullptr);
            depthSampleImageMemory = VK_NULL_HANDLE;
        }

        if (depthImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(logicalDevice, depthImageView, nullptr);
            depthImageView = VK_NULL_HANDLE;
        }

        if (depthImage != VK_NULL_HANDLE) {
            vkDestroyImage(logicalDevice, depthImage, nullptr);
            depthImage = VK_NULL_HANDLE;
        }

        if (depthImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(logicalDevice, depthImageMemory, nullptr);
            depthImageMemory = VK_NULL_HANDLE;
        }
    }

    // Helper: query whether the physical device supports any depth-resolve modes
    static VkResolveModeFlagBits chooseDepthResolveModeForPhysicalDevice(VkPhysicalDevice physicalDevice) {
        VkPhysicalDeviceDepthStencilResolveProperties dsResolveProps{};
        dsResolveProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES;
        dsResolveProps.pNext = nullptr;

        VkPhysicalDeviceProperties2 props2{};
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props2.pNext = &dsResolveProps;

        // This call is guaranteed to exist on recent SDKs; the runtime will fill dsResolveProps
        vkGetPhysicalDeviceProperties2(physicalDevice, &props2);

        if (dsResolveProps.supportedDepthResolveModes & VK_RESOLVE_MODE_SAMPLE_ZERO_BIT) {
            return VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
        }

        // no supported depth resolve modes
        return VK_RESOLVE_MODE_NONE;
    }

    void DepthManager::createDepthResources(uint32_t width, uint32_t height, VkSampleCountFlagBits msaaSamplesIn) {
        // Clean up existing resources first
        cleanup();

        this->msaaSamples = msaaSamplesIn;

        depthFormat = findDepthFormat();

        // Query depth-resolve support on this GPU
        VkResolveModeFlagBits mode = chooseDepthResolveModeForPhysicalDevice(vulkanDevice.physicalDevice);
        depthResolveSupported = (mode != VK_RESOLVE_MODE_NONE);

        // If MSAA disabled (1 sample) create one depth image that is both attachment + sampled
        if (msaaSamples == VK_SAMPLE_COUNT_1_BIT) {
            // Create a single image usable as depth attachment and sampled image
            createImage(width, height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        depthImage, depthImageMemory, VK_SAMPLE_COUNT_1_BIT);

            depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

            // Use same image for sampling view (no separate resolve image needed)
            depthSampleImage = depthImage;
            depthSampleImageMemory = depthImageMemory;
            depthSamplerView = depthImageView;
        } else {
            // MSAA > 1: create a multisampled depth attachment image
            // Note: we intentionally *do not* request SAMPLED_BIT here. If you later want to implement a shader-based manual resolve,
            // you might need to create the multisampled depth with SAMPLED_BIT so you can sample with sampler2DMS in a shader.
            createImage(width, height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        depthImage, depthImageMemory, msaaSamples);

            depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

            if (depthResolveSupported) {
                // Create single-sample resolve image that will be written by the renderpass depth-resolve
                createImage(width, height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                            depthSampleImage, depthSampleImageMemory, VK_SAMPLE_COUNT_1_BIT);

                depthSamplerView = createImageView(depthSampleImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
            } else {
                // No hardware depth resolve: don't create resolve image here.
                // The application must perform a manual resolve (blit/compute/shader) if it needs a single-sample depth sampler.
                depthSampleImage = VK_NULL_HANDLE;
                depthSampleImageMemory = VK_NULL_HANDLE;
                depthSamplerView = VK_NULL_HANDLE;
            }
        }

        createDepthSampler();
    }

    VkFormat DepthManager::findDepthFormat() {
        return findSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
        );
    }

    VkFormat DepthManager::findSupportedFormat( const std::vector<VkFormat>& candidates,
                                                VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(vulkanDevice.physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format!");
    }

    bool DepthManager::hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    void DepthManager::createImage( uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                                    VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                                    VkImage& image, VkDeviceMemory& imageMemory, VkSampleCountFlagBits numSamples) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = numSamples;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(vulkanDevice.device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create depth image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(vulkanDevice.device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = Device::findMemoryType(memRequirements.memoryTypeBits, properties, vulkanDevice.physicalDevice);

        if (vkAllocateMemory(vulkanDevice.device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate depth image memory!");
        }

        vkBindImageMemory(vulkanDevice.device, image, imageMemory, 0);
    }

    VkImageView DepthManager::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        if (vkCreateImageView(vulkanDevice.device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create depth image view!");
        }

        return imageView;
    }

    void DepthManager::createDepthSampler() {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;  // For normal depth sampling, not shadow mapping
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        samplerInfo.mipLodBias = 0.0f;

        if (vkCreateSampler(vulkanDevice.device, &samplerInfo, nullptr, &depthSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create depth sampler!");
        }
    }

    // Ensure the depth image usable for sampling is in DEPTH_STENCIL_READ_ONLY_OPTIMAL and synchronized
    // If MSAA > 1 we expect the renderpass to resolve to depthSampleImage (single-sample) already if supported,
    // but perform a safe barrier to make sure availability for fragment shader reads. If hardware resolve isn't supported,
    // a manual resolve must be performed prior to sampling (see TODO).
    void DepthManager::copyDepthForSampling(VkCommandBuffer commandBuffer, uint32_t width, uint32_t height) {
        // Determine which image we will use for sampling:
        // - msaa == 1: depthImage is the sampled image
        // - msaa > 1 && depthResolveSupported: depthSampleImage is the resolved single-sample image
        // - msaa > 1 && !depthResolveSupported: no automatic resolved image exists; manual resolve is required.
        VkImage targetImage = VK_NULL_HANDLE;

        if (msaaSamples == VK_SAMPLE_COUNT_1_BIT) {
            targetImage = depthImage;
        } else if (depthResolveSupported) {
            targetImage = depthSampleImage;
        } else {
            // No hardware resolve support — TODO: perform manual resolve here (blit/compute). For now, we log and return.
            // Manual resolve approaches:
            //  - Create the MSAA depth image with VK_IMAGE_USAGE_SAMPLED_BIT and sample it in a fullscreen compute shader
            //    (sampler2DMS) that writes the resolved depth into a single-sample R32_SFLOAT image (with STORAGE usage).
            //  - Or run a fullscreen fragment shader that reads the MSAA depth (if created with SAMPLED_BIT) and writes depth
            //    into a single-sample color image (R32) via color output. That color image can then be sampled.
            // Note: these approaches require additional pipelines/shaders and slightly different image usage flags.
            std::fprintf(stderr, "[DepthManager] Warning: depth resolve not supported by hardware and no manual resolve implemented.\n");
            return;
        }

        if (targetImage == VK_NULL_HANDLE) {
            return;
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = targetImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        // Renderpass wrote to depth -> make available to shader read
        barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        // If multisample and hardware-resolve supported, the renderpass typically finalizes the resolve image into
        // DEPTH_STENCIL_READ_ONLY_OPTIMAL; in single-sample case it may leave it as DEPTH_STENCIL_ATTACHMENT_OPTIMAL.
        VkImageLayout assumedOldLayout = (msaaSamples == VK_SAMPLE_COUNT_1_BIT)
            ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        barrier.oldLayout = assumedOldLayout;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        VkPipelineStageFlags dstStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            srcStages,
            dstStages,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
    }
} // namespace AzVulk
