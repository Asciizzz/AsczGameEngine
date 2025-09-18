// DepthManager.cpp
#include "AzVulk/DepthManager.hpp"
#include "AzVulk/Device.hpp"

#include <stdexcept>
#include <algorithm>
#include <cstdio>
#include <cstring>

using namespace AzVulk;

DepthManager::DepthManager(const Device* deviceVK)
    : deviceVK   (deviceVK)
    , depthImage (deviceVK)
{}

void DepthManager::createDepthResources(uint32_t width, uint32_t height) {
    depthImage.cleanup(); // In case of re-creation

    ImageConfig depthConfig = ImageConfig()
        .setDimensions(width, height)
        .setFormat(findDepthFormat())
        .setUsage(ImageUsageAlias::DepthStencil | ImageUsageAlias::Sampled);

    ImageViewConfig depthViewConfig = ImageViewConfig()
        .setAspectMask(VK_IMAGE_ASPECT_DEPTH_BIT);

    depthImage.createImage(depthConfig);
    depthImage.createImageView(depthViewConfig);
}

VkFormat DepthManager::findDepthFormat() {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
    );
}

VkFormat DepthManager::findSupportedFormat(
    const std::vector<VkFormat>& candidates,
    VkImageTiling tiling, VkFormatFeatureFlags features) 
{
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(deviceVK->pDevice, format, &props);

        VkFormatFeatureFlags supported =
            tiling == VK_IMAGE_TILING_LINEAR ? props.linearTilingFeatures
                                            :  props.optimalTilingFeatures;

        if ((supported & features) == features) return format;
    }

    throw std::runtime_error("Failed to find supported format!");
}
