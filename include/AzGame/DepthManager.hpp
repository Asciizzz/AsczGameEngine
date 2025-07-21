#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace AzGame {
    class VulkanDevice;

    class DepthManager {
    public:
        DepthManager(const VulkanDevice& device);
        ~DepthManager();

        // Delete copy constructor and assignment operator
        DepthManager(const DepthManager&) = delete;
        DepthManager& operator=(const DepthManager&) = delete;

        void createDepthResources(uint32_t width, uint32_t height);
        void cleanup();

        VkImageView getDepthImageView() const { return depthImageView; }
        VkFormat getDepthFormat() const { return depthFormat; }

    private:
        const VulkanDevice& vulkanDevice;
        
        VkImage depthImage = VK_NULL_HANDLE;
        VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
        VkImageView depthImageView = VK_NULL_HANDLE;
        VkFormat depthFormat;

        VkFormat findDepthFormat();
        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, 
                                   VkImageTiling tiling, VkFormatFeatureFlags features);
        bool hasStencilComponent(VkFormat format);
        void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                        VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                        VkImage& image, VkDeviceMemory& imageMemory);
        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    };
}
