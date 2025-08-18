#pragma once

#include "Helpers/Templates.hpp"

#include <vulkan/vulkan.h>
#include <optional>
#include <set>

namespace AzVulk {
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        std::optional<uint32_t> transferFamily;
        std::optional<uint32_t> computeFamily;

        // Graphic and present are the minimum required queue families
        bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    class Device {
    public:
        static const std::vector<const char*> deviceExtensions;

        Device(VkInstance instance, VkSurfaceKHR surface);
        ~Device();

        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;

        // Memory and buffer utilities
        static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice);

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
        VkQueue transferQueue = VK_NULL_HANDLE;
        VkQueue computeQueue = VK_NULL_HANDLE;
        QueueFamilyIndices queueFamilyIndices;

        enum QueueFamilyType {
            GraphicsQueueType,
            PresentQueueType,
            TransferQueueType,
            ComputeQueueType
        };

        struct PoolWrapper {
            VkCommandPool pool;
            QueueFamilyType type;
        };

        uint32_t getGraphicsQueueFamilyIndex() const;
        uint32_t getPresentQueueFamilyIndex() const;
        uint32_t getTransferQueueFamilyIndex() const;
        uint32_t getComputeQueueFamilyIndex() const;
        uint32_t getQueueFamilyIndex(QueueFamilyType type) const;

        VkQueue getQueue(QueueFamilyType type) const;

        UnorderedStringMap<PoolWrapper> commandPools;
        VkCommandPool getCommandPool(std::string name) const;

        // Command pools
        VkCommandPool createCommandPool(std::string name, QueueFamilyType type, VkCommandPoolCreateFlags flags = 0);
        VkCommandBuffer beginSingleTimeCommands(std::string name) const;

        // Currently not correct
        void endSingleTimeCommands(std::string name, VkCommandBuffer commandBuffer) const;
    };
}
