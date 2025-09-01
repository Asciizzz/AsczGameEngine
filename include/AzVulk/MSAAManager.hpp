#pragma once

#include <vulkan/vulkan.h>

namespace AzVulk {
    class Device;

    class MSAAManager {
    public:
        MSAAManager(const Device* vkDevice);
        ~MSAAManager();

        
        MSAAManager(const MSAAManager&) = delete;
        MSAAManager& operator=(const MSAAManager&) = delete;

        void createColorResources(uint32_t width, uint32_t height, VkFormat colorFormat);
        void cleanup();

        
        const Device* vkDevice;
        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        bool hasMSAA = false;

        VkImage colorImage = VK_NULL_HANDLE;
        VkDeviceMemory colorImageMemory = VK_NULL_HANDLE;
        VkImageView colorImageView = VK_NULL_HANDLE;

        // Helper methods 
        VkSampleCountFlagBits getMaxUsableSampleCount();
        void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, 
                        VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, 
                        VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
    };
}
