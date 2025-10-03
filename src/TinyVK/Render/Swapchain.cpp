#include "TinyVK/Render/Swapchain.hpp"

#include <SDL2/SDL_vulkan.h>
#include <stdexcept>
#include <algorithm>
#include <limits>

using namespace TinyVK;

Swapchain::Swapchain(const Device* deviceVK, VkSurfaceKHR surface, SDL_Window* window)
    : deviceVK(deviceVK), surface(surface) {
    createSwapChain(window);
    createImageViews();
}

Swapchain::~Swapchain() {
    cleanup();
}

void Swapchain::createSwapChain(SDL_Window* window) {
    SwapChainSupportDetails swapchainSupport = querySwapChainSupport(deviceVK->pDevice);

    VkSurfaceFormatKHR sc_surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);
    VkPresentModeKHR sc_presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);
    VkExtent2D sc_extent = chooseSwapExtent(swapchainSupport.capabilities, window);

    uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
    if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount) {
        imageCount = swapchainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = sc_surfaceFormat.format;
    createInfo.imageColorSpace = sc_surfaceFormat.colorSpace;
    createInfo.imageExtent = sc_extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    QueueFamilyIndices indices = deviceVK->queueFamilyIndices;
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = sc_presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(deviceVK->device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(deviceVK->device, swapchain, &imageCount, nullptr);

    std::vector<VkImage> rawImages(imageCount);
    vkGetSwapchainImagesKHR(deviceVK->device, swapchain, &imageCount, rawImages.data());

    for (const auto& image : rawImages) {
        // images.emplace_back(deviceVK->device);
        ImageVK img = ImageVK(deviceVK->device);
        img.setSwapchainImage(image, sc_surfaceFormat.format, sc_extent);

        images.push_back(std::move(img));
    }
}

void Swapchain::createImageViews() {
    ImageViewConfig viewConfig = ImageViewConfig()
        .withType(VK_IMAGE_VIEW_TYPE_2D)
        .withAspectMask(ImageAspect::Color)
        .withMipLevels(1)
        .withArrayLayers(1);

    for (auto& img : images) {
        img.createView(viewConfig);
    }
}


void Swapchain::cleanup() {
    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(deviceVK->device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }

    images.clear();
}

SwapChainSupportDetails Swapchain::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR Swapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && 
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR Swapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Swapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width, height;
    SDL_Vulkan_GetDrawableSize(window, &width, &height);

    VkExtent2D actualExtent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    actualExtent.width = std::clamp(actualExtent.width, 
        capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, 
        capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
}
