#include "AzVulk/Device.hpp"
#include "AzVulk/SwapChain.hpp"
#include <stdexcept>
#include <set>

namespace AzVulk
{
    const std::vector<const char *> Device::deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        // Render pass 2 khr
        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME
    };

    Device::Device(VkInstance instance, VkSurfaceKHR surface) {
        pickPhysicalDevice(instance, surface);
        createLogicalDevice();
    }

    Device::~Device() {
        // Destroy command pools
        for (const auto &[name, poolWrapper] : commandPools) {
            vkDestroyCommandPool(device, poolWrapper.pool, nullptr);
        }

        if (device != VK_NULL_HANDLE) vkDestroyDevice(device, nullptr);
    }

    void Device::pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto &device : devices) {
            if (isDeviceSuitable(device, surface)) {
                physicalDevice = device;
                queueFamilyIndices = findQueueFamilies(device, surface);
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void Device::createLogicalDevice() {
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {
            queueFamilyIndices.graphicsFamily.value(),
            queueFamilyIndices.presentFamily.value(),
            queueFamilyIndices.transferFamily.value(),
            queueFamilyIndices.computeFamily.value()
        };

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE; // Enable anisotropic filtering
        deviceFeatures.sampleRateShading = VK_TRUE; // Enable sample shading feature for the device

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        createInfo.enabledLayerCount = 0;

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(device, queueFamilyIndices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, queueFamilyIndices.presentFamily.value(), 0, &presentQueue);
        vkGetDeviceQueue(device, queueFamilyIndices.transferFamily.value(), 0, &transferQueue);
        vkGetDeviceQueue(device, queueFamilyIndices.computeFamily.value(), 0, &computeQueue);
    }

    bool Device::isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
        QueueFamilyIndices indices = findQueueFamilies(device, surface);
        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport;

            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapChainSupport.capabilities);

            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
            if (formatCount != 0) {
                swapChainSupport.formats.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapChainSupport.formats.data());
            }

            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
            if (presentModeCount != 0) {
                swapChainSupport.presentModes.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, swapChainSupport.presentModes.data());
            }

            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    bool Device::checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto &extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            // Graphics
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            // Present
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
            }

            // Compute (prefer a dedicated compute queue if possible)
            if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                indices.computeFamily = i;
            }

            // Transfer (prefer a dedicated transfer queue if possible)
            if ((queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                !(queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                indices.transferFamily = i;
            }

            i++;
        }

        // Fallbacks: if no dedicated compute/transfer queue was found, use graphics
        if (!indices.computeFamily.has_value()) {
            indices.computeFamily = indices.graphicsFamily;
        }
        if (!indices.transferFamily.has_value()) {
            indices.transferFamily = indices.graphicsFamily;
        }

        return indices;
    }

    uint32_t Device::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }



    VkCommandPool Device::createCommandPool(std::string name, QueueFamilyType type, VkCommandPoolCreateFlags flags) {
        if (commandPools.find(name) != commandPools.end()) {
            throw std::runtime_error("Command pool with name '" + name + "' already exists!");
        }

        

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = getQueueFamilyIndex(type);
        poolInfo.flags = flags;

        VkCommandPool commandPool;
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
        commandPools[name] = { commandPool, type };
        return commandPool;
    }



    TemporaryCommand::TemporaryCommand(const Device& device, const std::string& poolName)
    : device(device), poolName(poolName) {
        if (device.commandPools.find(poolName) == device.commandPools.end()) {
            throw std::runtime_error("Command pool not found!");
        }

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = device.getPoolWrapper(poolName).pool;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device.device, &allocInfo, &cmdBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffer!");
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    }

    TemporaryCommand::~TemporaryCommand() {
        if (cmdBuffer != VK_NULL_HANDLE) {
            vkEndCommandBuffer(cmdBuffer);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmdBuffer;

            const Device::PoolWrapper& poolWrapper = device.getPoolWrapper(poolName);

            vkQueueSubmit(device.getQueue(poolWrapper.type), 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(device.getQueue(poolWrapper.type));

            vkFreeCommandBuffers(device.device, poolWrapper.pool, 1, &cmdBuffer);
        }
    }
}
