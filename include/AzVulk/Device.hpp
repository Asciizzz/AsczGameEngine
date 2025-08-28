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

    bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

class Device {
public:
    static const std::vector<const char*> deviceExtensions;

    Device(VkInstance instance, VkSurfaceKHR surface);
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // Vulkan objects
    VkPhysicalDevice pDevice = VK_NULL_HANDLE;
    VkDevice lDevice = VK_NULL_HANDLE;

    // Queues
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue  = VK_NULL_HANDLE;
    VkQueue transferQueue = VK_NULL_HANDLE;
    VkQueue computeQueue  = VK_NULL_HANDLE;

    QueueFamilyIndices queueFamilyIndices;

    enum QueueFamilyType { GraphicsType, PresentType, TransferType, ComputeType };

    uint32_t getQueueFamilyIndex(QueueFamilyType type) const;
    VkQueue getQueue(QueueFamilyType type) const;

    struct PoolWrapper { VkCommandPool pool; QueueFamilyType type; };

    PoolWrapper graphicsPoolWrapper{};
    PoolWrapper presentPoolWrapper{};
    PoolWrapper transferPoolWrapper{};
    PoolWrapper computePoolWrapper{};

    void createDefaultCommandPools();
    PoolWrapper createCommandPool(QueueFamilyType type, VkCommandPoolCreateFlags flags = 0);

    static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice pDevice);

private:
    void pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
    void createLogicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice lDevice, VkSurfaceKHR surface);
    bool checkDeviceExtensionSupport(VkPhysicalDevice lDevice);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice lDevice, VkSurfaceKHR surface);
};

// RAII temporary command buffer wrapper
class TemporaryCommand {
public:
    TemporaryCommand(const Device* vkDevice, const Device::PoolWrapper& poolWrapper);
    ~TemporaryCommand();
    void endAndSubmit(VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    VkCommandBuffer get() const { return cmdBuffer; }

    const Device* vkDevice = nullptr;
    Device::PoolWrapper poolWrapper{};
    VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
    bool submitted = false;
};

} // namespace AzVulk
