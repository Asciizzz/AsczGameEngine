#pragma once

#include "AzVulk/TextureVK.hpp"

namespace AzVulk {
    class Device;

    class DepthManager {
    public:
        DepthManager(const Device* deviceVK);
        ~DepthManager() = default;

        DepthManager(const DepthManager&) = delete;
        DepthManager& operator=(const DepthManager&) = delete;

        void createDepthResources(uint32_t width, uint32_t height);

        // Accessors for depth resources
        VkImageView getDepthImageView() const { return depthImage.getView(); }
        VkImage getDepthImage() const { return depthImage.getImage(); }
        uint32_t getWidth() const { return depthImage.getWidth(); }
        uint32_t getHeight() const { return depthImage.getHeight(); }

        const Device* deviceVK;
        
        // Modern ImageVK-based depth resource
        ImageVK depthImage;

        VkFormat depthFormat;

        // Helper methods
        VkFormat findDepthFormat();
        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, 
                                    VkImageTiling tiling, VkFormatFeatureFlags features);
    };
}
