#include "TinyVK/SwapChain.hpp"
#include <SDL2/SDL_vulkan.h>
#include <stdexcept>
#include <algorithm>
#include <limits>

using namespace TinyVK;

SwapChain::SwapChain(const DeviceVK* deviceVK, VkSurfaceKHR surface, SDL_Window* window)
    : deviceVK(deviceVK), surface(surface) {
    createSwapChain(window);
    createImageViews();
}

SwapChain::~SwapChain() {
    cleanup();
}

void SwapChain::createSwapChain(SDL_Window* window) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(deviceVK->pDevice);

    VkSurfaceFormatKHR sc_surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR sc_presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D sc_extent = chooseSwapExtent(swapChainSupport.capabilities, window);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
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

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = sc_presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(deviceVK->lDevice, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(deviceVK->lDevice, swapChain, &imageCount, nullptr);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(deviceVK->lDevice, swapChain, &imageCount, images.data());

    imageFormat = sc_surfaceFormat.format;
    extent = sc_extent;
}

void SwapChain::createImageViews() {
    imageViews.resize(images.size());

    for (size_t i = 0; i < images.size(); ++i) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = imageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(deviceVK->lDevice, &createInfo, nullptr, &imageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image views!");
        }
    }
}

void SwapChain::createFramebuffers(VkRenderPass renderPass, VkImageView depthImageView) {
    framebuffers.clear();

    for (size_t i = 0; i < imageViews.size(); ++i) {
        std::vector<VkImageView> attachments = {
            imageViews[i],   // swapchain color
            depthImageView   // depth
        };

        // VkFramebufferCreateInfo framebufferInfo{};
        // framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        // framebufferInfo.renderPass = renderPass;
        // framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        // framebufferInfo.pAttachments = attachments.data();
        // framebufferInfo.width = extent.width;
        // framebufferInfo.height = extent.height;
        // framebufferInfo.layers = 1;

        // if (vkCreateFramebuffer(deviceVK->lDevice, &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to create framebuffer!");
        // }

        FrameBufferConfig fbConfig = FrameBufferConfig()
            .withRenderPass(renderPass)
            .withAttachments(attachments)
            .withExtent(extent);

        UniquePtr<FrameBuffer> framebuffer = std::make_unique<FrameBuffer>();
        bool success = framebuffer->create(deviceVK->lDevice, fbConfig);
        if (!success) throw std::runtime_error("Failed to create framebuffer");

        framebuffers.push_back(std::move(framebuffer));
    }
}

void SwapChain::recreateFramebuffers(SDL_Window* window, VkRenderPass renderPass, VkImageView depthImageView) {
    int width = 0, height = 0;
    SDL_GetWindowSize(window, &width, &height);
    while (width == 0 || height == 0) {
        SDL_GetWindowSize(window, &width, &height);
        SDL_WaitEvent(nullptr);
    }

    vkDeviceWaitIdle(deviceVK->lDevice);

    cleanup();

    createSwapChain(window);
    createImageViews();
    createFramebuffers(renderPass, depthImageView);
}

void SwapChain::cleanup() {
    framebuffers.clear();

    for (auto view : imageViews) {
        vkDestroyImageView(deviceVK->lDevice, view, nullptr);
    }
    imageViews.clear();

    if (swapChain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(deviceVK->lDevice, swapChain, nullptr);
        swapChain = VK_NULL_HANDLE;
    }
}

SwapChainSupportDetails SwapChain::querySwapChainSupport(VkPhysicalDevice lDevice) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(lDevice, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(lDevice, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(lDevice, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(lDevice, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(lDevice, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && 
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window) {
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
