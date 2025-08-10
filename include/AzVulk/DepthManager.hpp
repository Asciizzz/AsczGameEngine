#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace AzVulk {
    class Device;

    class DepthManager {
    public:
        DepthManager(const Device& device);
        ~DepthManager();

        
        DepthManager(const DepthManager&) = delete;
        DepthManager& operator=(const DepthManager&) = delete;

        void createDepthResources(uint32_t width, uint32_t height, VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT);
        void cleanup();

        const Device& vulkanDevice;
        
        VkImage depthImage = VK_NULL_HANDLE;
        VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
        VkImageView depthImageView = VK_NULL_HANDLE;
        VkFormat depthFormat;

        // Helper methods 
        VkFormat findDepthFormat();
        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, 
                                    VkImageTiling tiling, VkFormatFeatureFlags features);
        bool hasStencilComponent(VkFormat format);
        void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                        VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                        VkImage& image, VkDeviceMemory& imageMemory, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT);
        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    };
}
