#pragma once

#include <vulkan/vulkan.h>

namespace AzVulk {
    class VulkanDevice;

    class MSAAManager {
    public:
        MSAAManager(const VulkanDevice& device);
        ~MSAAManager();

        // Delete copy constructor and assignment operator
        MSAAManager(const MSAAManager&) = delete;
        MSAAManager& operator=(const MSAAManager&) = delete;

        void createColorResources(uint32_t width, uint32_t height, VkFormat colorFormat);
        void cleanup();

        
        const VulkanDevice& vulkanDevice;
        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

        VkImage colorImage = VK_NULL_HANDLE;
        VkDeviceMemory colorImageMemory = VK_NULL_HANDLE;
        VkImageView colorImageView = VK_NULL_HANDLE;

        // Helper methods (now public for direct access)
        VkSampleCountFlagBits getMaxUsableSampleCount();
        void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, 
                        VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, 
                        VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
    };
}
