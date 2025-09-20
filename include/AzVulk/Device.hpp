#pragma once

#include "Helpers/Templates.hpp"

#include <vulkan/vulkan.h>
#include <optional>
#include <set>

namespace AzVulk {

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

struct FormatAlias {
    static constexpr VkFormat RGBA8Unorm   = VK_FORMAT_R8G8B8A8_UNORM;
    static constexpr VkFormat RGBA8Srgb    = VK_FORMAT_R8G8B8A8_SRGB;
    
    static constexpr VkFormat BGRA8Unorm   = VK_FORMAT_B8G8R8A8_UNORM;
    static constexpr VkFormat BGRA8Srgb    = VK_FORMAT_B8G8R8A8_SRGB;

    static constexpr VkFormat R8Unorm      = VK_FORMAT_R8_UNORM;
    static constexpr VkFormat RG8Unorm     = VK_FORMAT_R8G8_UNORM;
    static constexpr VkFormat R32Sfloat    = VK_FORMAT_R32_SFLOAT;

    static constexpr VkFormat D32Sfloat    = VK_FORMAT_D32_SFLOAT;
    static constexpr VkFormat D24UnormS8   = VK_FORMAT_D24_UNORM_S8_UINT;

    static constexpr VkFormat RGBA16Sfloat = VK_FORMAT_R16G16B16A16_SFLOAT;
    static constexpr VkFormat RGBA32Sfloat = VK_FORMAT_R32G32B32A32_SFLOAT;

    static constexpr VkFormat RGB16Sfloat  = VK_FORMAT_R16G16B16_SFLOAT;
    static constexpr VkFormat RGB32Sfloat  = VK_FORMAT_R32G32B32_SFLOAT;

    static constexpr VkFormat R16Sfloat    = VK_FORMAT_R16_SFLOAT;
    static constexpr VkFormat R32Uint      = VK_FORMAT_R32_UINT;
    static constexpr VkFormat R32Sint      = VK_FORMAT_R32_SINT;
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
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

private:
    void pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
    void createLogicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice lDevice, VkSurfaceKHR surface);
    bool checkDeviceExtensionSupport(VkPhysicalDevice lDevice);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice lDevice, VkSurfaceKHR surface);
};

} // namespace AzVulk
