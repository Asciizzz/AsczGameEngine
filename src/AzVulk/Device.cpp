#include "Device.hpp"
#include <stdexcept>
#include <set>
#include <vector>

using namespace AzVulk;

// ---------------- EXTENSIONS ----------------
const std::vector<const char*> Device::deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
};

// ---------------- CONSTRUCTOR / DESTRUCTOR ----------------
Device::Device(VkInstance instance, VkSurfaceKHR surface) {
    pickPhysicalDevice(instance, surface);
    createLogicalDevice();
    createDefaultCommandPools();
}

Device::~Device() {
    if (lDevice != VK_NULL_HANDLE) {
        vkDestroyCommandPool(lDevice, graphicsPoolWrapper.pool, nullptr);
        vkDestroyCommandPool(lDevice, presentPoolWrapper.pool, nullptr);
        vkDestroyCommandPool(lDevice, computePoolWrapper.pool, nullptr);
        vkDestroyCommandPool(lDevice, transferPoolWrapper.pool, nullptr);
        vkDestroyDevice(lDevice, nullptr);
    }
}

// ---------------- PHYSICAL DEVICE ----------------
void Device::pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (count == 0) throw std::runtime_error("No Vulkan devices found.");

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    for (auto& dev : devices) {
        if (isDeviceSuitable(dev, surface)) {
            pDevice = dev;
            queueFamilyIndices = findQueueFamilies(dev, surface);
            return;
        }
    }

    throw std::runtime_error("No suitable Vulkan GPU found.");
}

// ---------------- LOGICAL DEVICE ----------------
void Device::createLogicalDevice() {
    std::set<uint32_t> uniqueFamilies = {
        queueFamilyIndices.graphicsFamily.value(),
        queueFamilyIndices.presentFamily.value(),
        queueFamilyIndices.transferFamily.value_or(queueFamilyIndices.graphicsFamily.value()),
        queueFamilyIndices.computeFamily.value_or(queueFamilyIndices.graphicsFamily.value())
    };

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        info.queueFamilyIndex = family;
        info.queueCount = 1;
        info.pQueuePriorities = &priority;
        queueInfos.push_back(info);
    }

    // Vulkan 1.2 core features
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.sampleRateShading = VK_TRUE;

    VkPhysicalDeviceVulkan12Features vk12Features{};
    vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12Features.descriptorIndexing = VK_TRUE;
    vk12Features.runtimeDescriptorArray = VK_TRUE;
    vk12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    vk12Features.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
    vk12Features.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
    vk12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;  // if you use runtime arrays
    vk12Features.descriptorBindingPartiallyBound = VK_TRUE;           // optional

    VkDeviceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    ci.pQueueCreateInfos = queueInfos.data();
    ci.pEnabledFeatures = &deviceFeatures;
    ci.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    ci.ppEnabledExtensionNames = deviceExtensions.data();
    ci.pNext = &vk12Features;

    if (vkCreateDevice(pDevice, &ci, nullptr, &lDevice) != VK_SUCCESS)
        throw std::runtime_error("Failed to create logical lDevice");

    // Queues
    vkGetDeviceQueue(lDevice, queueFamilyIndices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(lDevice, queueFamilyIndices.presentFamily.value(), 0, &presentQueue);
    vkGetDeviceQueue(lDevice, queueFamilyIndices.transferFamily.value_or(queueFamilyIndices.graphicsFamily.value()), 0, &transferQueue);
    vkGetDeviceQueue(lDevice, queueFamilyIndices.computeFamily.value_or(queueFamilyIndices.graphicsFamily.value()), 0, &computeQueue);
}

// ---------------- QUEUE FAMILY ----------------
QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice lDevice, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(lDevice, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(lDevice, &count, families.data());

    int i = 0;
    for (auto& f : families) {
        if (f.queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphicsFamily = i;

        VkBool32 present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(lDevice, i, surface, &present);
        if (present) indices.presentFamily = i;

        if ((f.queueFlags & VK_QUEUE_COMPUTE_BIT) && !(f.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            indices.computeFamily = i;

        if ((f.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            !(f.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            !(f.queueFlags & VK_QUEUE_COMPUTE_BIT))
            indices.transferFamily = i;

        i++;
    }

    if (!indices.computeFamily.has_value()) indices.computeFamily = indices.graphicsFamily;
    if (!indices.transferFamily.has_value()) indices.transferFamily = indices.graphicsFamily;

    return indices;
}

bool Device::isDeviceSuitable(VkPhysicalDevice lDevice, VkSurfaceKHR surface) {
    QueueFamilyIndices indices = findQueueFamilies(lDevice, surface);
    bool extensionsOk = checkDeviceExtensionSupport(lDevice);

    uint32_t fmtCount = 0, presentCount = 0;
    if (extensionsOk) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(lDevice, surface, &fmtCount, nullptr);
        vkGetPhysicalDeviceSurfacePresentModesKHR(lDevice, surface, &presentCount, nullptr);
    }

    return indices.isComplete() && extensionsOk && fmtCount > 0 && presentCount > 0;
}

bool Device::checkDeviceExtensionSupport(VkPhysicalDevice lDevice) {
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(lDevice, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> available(extCount);
    vkEnumerateDeviceExtensionProperties(lDevice, nullptr, &extCount, available.data());

    std::set<std::string> required(deviceExtensions.begin(), deviceExtensions.end());
    for (auto& ext : available) required.erase(ext.extensionName);
    return required.empty();
}

// ---------------- COMMAND POOLS ----------------
Device::PoolWrapper Device::createCommandPool(QueueFamilyType type, VkCommandPoolCreateFlags flags) {
    VkCommandPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.queueFamilyIndex = getQueueFamilyIndex(type);
    ci.flags = flags;

    VkCommandPool pool;
    if (vkCreateCommandPool(lDevice, &ci, nullptr, &pool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool");

    return { pool, type };
}

void Device::createDefaultCommandPools() {
    graphicsPoolWrapper = createCommandPool(GraphicsType, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    presentPoolWrapper  = createCommandPool(PresentType, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    computePoolWrapper  = createCommandPool(ComputeType, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    transferPoolWrapper = createCommandPool(TransferType, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
}

// ---------------- MEMORY ----------------
uint32_t Device::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags memPropFlags, VkPhysicalDevice pDevice) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(pDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & memPropFlags) == memPropFlags)
            return i;

    throw std::runtime_error("Failed to find suitable memory type");
}

uint32_t Device::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags memPropFlags) const {
    return findMemoryType(typeFilter, memPropFlags, pDevice);
}

// ---------------- QUEUE HELPERS ----------------
uint32_t Device::getQueueFamilyIndex(QueueFamilyType type) const {
    switch(type) {
        case GraphicsType: return queueFamilyIndices.graphicsFamily.value();
        case PresentType:  return queueFamilyIndices.presentFamily.value();
        case TransferType: return queueFamilyIndices.transferFamily.value_or(queueFamilyIndices.graphicsFamily.value());
        case ComputeType:  return queueFamilyIndices.computeFamily.value_or(queueFamilyIndices.graphicsFamily.value());
    }
    return queueFamilyIndices.graphicsFamily.value();
}

VkQueue Device::getQueue(QueueFamilyType type) const {
    switch(type) {
        case GraphicsType: return graphicsQueue;
        case PresentType:  return presentQueue;
        case TransferType: return transferQueue;
        case ComputeType:  return computeQueue;
    }
    return graphicsQueue;
}

// ---------------- TEMPORARY COMMAND BUFFER ----------------
TemporaryCommand::TemporaryCommand(const Device* dev, const Device::PoolWrapper& pool) 
    : vkDevice(dev), poolWrapper(pool) {
    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandPool = poolWrapper.pool;
    alloc.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(vkDevice->lDevice, &alloc, &cmdBuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffer");

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuffer, &begin);
}

TemporaryCommand::~TemporaryCommand() {
    if (cmdBuffer != VK_NULL_HANDLE) {
        if (!submitted) endAndSubmit();
        vkFreeCommandBuffers(vkDevice->lDevice, poolWrapper.pool, 1, &cmdBuffer);
    }
}

void TemporaryCommand::endAndSubmit(VkPipelineStageFlags waitStage) {
    if (submitted) return;
    submitted = true;

    vkEndCommandBuffer(cmdBuffer);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmdBuffer;

    vkQueueSubmit(vkDevice->getQueue(poolWrapper.type), 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(vkDevice->getQueue(poolWrapper.type));

    cmdBuffer = VK_NULL_HANDLE;
}
