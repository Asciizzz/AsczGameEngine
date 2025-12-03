#pragma once

#include "Templates.hpp"

#include <vulkan/vulkan.h>
#include <optional>
#include <set>

namespace tinyVk {

struct MemProp {
    static constexpr VkMemoryPropertyFlags DeviceLocal  = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    static constexpr VkMemoryPropertyFlags HostVisible  = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    static constexpr VkMemoryPropertyFlags HostCoherent = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    static constexpr VkMemoryPropertyFlags HostCached   = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

    static constexpr VkMemoryPropertyFlags HostVisibleAndCoherent = HostVisible | HostCoherent;
};

struct ShaderStage {
    static constexpr VkShaderStageFlags Vertex   = VK_SHADER_STAGE_VERTEX_BIT;
    static constexpr VkShaderStageFlags Fragment = VK_SHADER_STAGE_FRAGMENT_BIT;
    static constexpr VkShaderStageFlags Compute  = VK_SHADER_STAGE_COMPUTE_BIT;
    static constexpr VkShaderStageFlags All      = VK_SHADER_STAGE_ALL;

    static constexpr VkShaderStageFlags VertexAndFragment = Vertex | Fragment;
};

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
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice pDevice = VK_NULL_HANDLE;

    // Queues
    
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue  = VK_NULL_HANDLE;
    VkQueue transferQueue = VK_NULL_HANDLE;
    VkQueue computeQueue  = VK_NULL_HANDLE;
    QueueFamilyIndices queueFamilyIndices;

    // Cached values
    VkPhysicalDeviceProperties pProps{};
    VkPhysicalDeviceMemoryProperties pMemProps{};
    VkPhysicalDeviceFeatures pFeatures{};

    // Helper functions
    size_t alignSize(size_t original, size_t minAlignment) const {
        size_t alignedSize = original;
        if (minAlignment > 0) {
            alignedSize = (alignedSize + minAlignment - 1) & ~(minAlignment - 1);
        }
        return alignedSize;
    }


    size_t alignSizeUBO(size_t originalSize) const {
        return alignSize(originalSize, pProps.limits.minUniformBufferOffsetAlignment);
    }

    size_t alignSizeSSBO(size_t originalSize) const {
        return alignSize(originalSize, pProps.limits.minStorageBufferOffsetAlignment);
    }


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
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

private:
    void pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
    void createLogicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
};

} // namespace tinyVk
