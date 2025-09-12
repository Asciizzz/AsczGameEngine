// DepthManager.cpp
#include "AzVulk/DepthManager.hpp"
#include "AzVulk/Device.hpp"

#include <stdexcept>
#include <algorithm>
#include <cstdio>
#include <cstring>

using namespace AzVulk;

DepthManager::DepthManager(const Device* deviceVK)
    : deviceVK              (deviceVK)
    , depthBuffer           (deviceVK)
    , depthResolveSupported (false)
{}

DepthManager::~DepthManager() {
    cleanup();
}

void DepthManager::cleanup() {
    // ImageVK handles cleanup automatically
    depthBuffer.cleanup();
}

void DepthManager::createDepthResources(uint32_t width, uint32_t height) {
    // Clean up existing resources first
    cleanup();

    depthFormat = findDepthFormat();

    // Create depth buffer using ImageVK convenience method
    if (!depthBuffer.createDepthBuffer(width, height, depthFormat)) {
        throw std::runtime_error("Failed to create depth buffer!");
    }
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
        vkGetPhysicalDeviceFormatProperties(deviceVK->pDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}
