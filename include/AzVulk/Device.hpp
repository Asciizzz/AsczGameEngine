#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <set>

namespace AzVulk {
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    class Device {
    public:
        Device(VkInstance instance, VkSurfaceKHR surface);
        ~Device();

        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;

        // Memory and buffer utilities
        static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice);
        
        // Legacy, moved to Buffer.cpp
        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const;
        void destroyBuffer(VkBuffer buffer, VkDeviceMemory bufferMemory) const;
        
        VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool) const;
        void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool commandPool) const;

        // Device setup methods
        void pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
        void createLogicalDevice();
        bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

        // Vulkan device objects
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkQueue graphicsQueue = VK_NULL_HANDLE;
        VkQueue presentQueue = VK_NULL_HANDLE;
        QueueFamilyIndices queueFamilyIndices;

        static const std::vector<const char*> deviceExtensions;
    };
}
