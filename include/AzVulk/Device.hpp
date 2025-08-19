#pragma once

#include <vulkan/vulkan.h>
#include <optional>
#include <set>

#include "Helpers/Templates.hpp"

namespace AzVulk {
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        std::optional<uint32_t> transferFamily;
        std::optional<uint32_t> computeFamily;

        bool isComplete() const { // Minimum requirements
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
            GraphicsType,
            PresentType,
            TransferType,
            ComputeType
        };

        uint32_t getGraphicsQueueFamilyIndex() const { return queueFamilyIndices.graphicsFamily.value(); }
        uint32_t getPresentQueueFamilyIndex() const { return queueFamilyIndices.presentFamily.value(); }
        uint32_t getTransferQueueFamilyIndex() const { return queueFamilyIndices.transferFamily.value_or(getGraphicsQueueFamilyIndex()); }
        uint32_t getComputeQueueFamilyIndex() const { return queueFamilyIndices.computeFamily.value_or(getGraphicsQueueFamilyIndex()); }
        uint32_t getQueueFamilyIndex(QueueFamilyType type) const {
            switch (type) {
                case GraphicsType: return getGraphicsQueueFamilyIndex();
                case PresentType: return getPresentQueueFamilyIndex();
                case TransferType: return getTransferQueueFamilyIndex();
                case ComputeType: return getComputeQueueFamilyIndex();
                default: return getGraphicsQueueFamilyIndex(); // Fallback
            }
        }
        VkQueue getQueue(QueueFamilyType type) const {
            switch (type) {
                case GraphicsType: return graphicsQueue;
                case PresentType: return presentQueue;
                case TransferType: return transferQueue;
                case ComputeType: return computeQueue;
                default: return graphicsQueue; // Fallback
            }
        }

        struct PoolWrapper { VkCommandPool pool; QueueFamilyType type; };
        UnorderedMap<std::string, PoolWrapper> commandPools; // Command pools
        VkCommandPool createCommandPool(std::string name, QueueFamilyType type, VkCommandPoolCreateFlags flags = 0);

        PoolWrapper getPoolWrapper(const std::string& name) const { return commandPools.at(name); }
        VkCommandPool getCommandPool(const std::string& name) const { return commandPools.at(name).pool; }
    };


    // Extension helper for one-time instant command
    class TemporaryCommand {
    public:
        TemporaryCommand(Device& device, const std::string& poolName);
        ~TemporaryCommand();

        VkCommandBuffer getCmdBuffer() const { return cmd; }
        VkCommandBuffer operator*() const { return cmd; }

        Device& device;
        std::string poolName;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
    };
}
