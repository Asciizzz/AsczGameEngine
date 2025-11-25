// DepthImage.cpp
#include "tinyVk/Render/DepthImage.hpp"

#include <stdexcept>

using namespace tinyVk;

void DepthImage::create(VkDevice device, VkPhysicalDevice pDevice, uint32_t width, uint32_t height) {
    cleanup(); // Clean up existing resources if any

    ImageConfig imgCfg;
    imgCfg.device = device;
    imgCfg.pDevice = pDevice;
    imgCfg.dimensions(width, height);
    imgCfg.format = findDepthFormat(pDevice);
    imgCfg.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    ImageViewConfig depthViewConfig;
    depthViewConfig.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    createImage(imgCfg);
    createView(depthViewConfig);
}


VkFormat DepthImage::findDepthFormat(VkPhysicalDevice pDevice) {
    std::vector<VkFormat> candidates = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(pDevice, format, &props);

        VkFormatFeatureFlags supported =
            tiling == VK_IMAGE_TILING_LINEAR ? props.linearTilingFeatures
                                            :  props.optimalTilingFeatures;

        if ((supported & features) == features) return format;
    }

    throw std::runtime_error("Failed to find supported depth format!");
}