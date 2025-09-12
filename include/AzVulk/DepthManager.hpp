#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "ImageWrapper.hpp"

namespace AzVulk {
    class Device;

    class DepthManager {
    public:
        DepthManager(const Device* deviceVK);
        ~DepthManager(); void cleanup();

        DepthManager(const DepthManager&) = delete;
        DepthManager& operator=(const DepthManager&) = delete;

        void createDepthResources(uint32_t width, uint32_t height);

        // Accessors for depth resources
        VkImageView getDepthImageView() const { return depthBuffer.getImageView(); }
        VkImage getDepthImage() const { return depthBuffer.getImage(); }
        uint32_t getWidth() const { return depthBuffer.getWidth(); }
        uint32_t getHeight() const { return depthBuffer.getHeight(); }

        const Device* deviceVK;
        
        // Modern ImageWrapper-based depth resource
        ImageWrapper depthBuffer;

        VkFormat depthFormat;
        bool depthResolveSupported;

        // Helper methods
        VkFormat findDepthFormat();
        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, 
                                    VkImageTiling tiling, VkFormatFeatureFlags features);
    };
}
