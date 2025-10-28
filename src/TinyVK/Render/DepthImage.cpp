// DepthImage.cpp
#include "tinyVK/Render/DepthImage.hpp"

#include <stdexcept>

using namespace tinyVK;

void DepthImage::create(VkPhysicalDevice pDevice, uint32_t width, uint32_t height) {
    cleanup(); // Clean up existing resources if any

    ImageConfig depthConfig = ImageConfig()
        .withPhysicalDevice(pDevice)
        .withDimensions(width, height)
        .withFormat(findDepthFormat(pDevice))
        .withUsage(ImageUsage::DepthStencil | ImageUsage::Sampled);

    ImageViewConfig depthViewConfig = ImageViewConfig()
        .withAspectMask(VK_IMAGE_ASPECT_DEPTH_BIT);

    createImage(depthConfig);
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