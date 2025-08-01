#include "AzVulk/SwapChain.hpp"
#include <SDL2/SDL_vulkan.h>
#include <stdexcept>
#include <algorithm>
#include <limits>
#include <array>

namespace AzVulk {
    SwapChain::SwapChain(const Device& device, VkSurfaceKHR surface, SDL_Window* window)
        : vulkanDevice(device), surface(surface) {
        createSwapChain(window);
        createImageViews();
    }

    SwapChain::~SwapChain() {
        cleanup();
    }

    void SwapChain::createSwapChain(SDL_Window* window) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(vulkanDevice.physicalDevice);

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
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = vulkanDevice.queueFamilyIndices;
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

        if (vkCreateSwapchainKHR(vulkanDevice.device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(vulkanDevice.device, swapChain, &imageCount, nullptr);
        images.resize(imageCount);
        vkGetSwapchainImagesKHR(vulkanDevice.device, swapChain, &imageCount, images.data());

        imageFormat = sc_surfaceFormat.format;
        extent = sc_extent;
    }

    void SwapChain::createImageViews() {
        imageViews.resize(images.size());

        for (size_t i = 0; i < images.size(); i++) {
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

            if (vkCreateImageView(vulkanDevice.device, &createInfo, nullptr, &imageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }
        }
    }

    void SwapChain::createFramebuffers(VkRenderPass renderPass, VkImageView depthImageView, VkImageView colorImageView) {
        cleanupFramebuffers();
        framebuffers.resize(imageViews.size());

        for (size_t i = 0; i < imageViews.size(); i++) {
            std::vector<VkImageView> attachments;
            
            if (colorImageView != VK_NULL_HANDLE) {
                // MSAA case: colorImageView, depthImageView, swapChainImageView (resolve target)
                attachments = {
                    colorImageView,
                    depthImageView,
                    imageViews[i]
                };
            } else {
                // Non-MSAA case: swapChainImageView, depthImageView
                attachments = {
                    imageViews[i],
                    depthImageView
                };
            }

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = extent.width;
            framebufferInfo.height = extent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(vulkanDevice.device, &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void SwapChain::recreate(SDL_Window* window, VkRenderPass renderPass, VkImageView depthImageView, VkImageView colorImageView) {
        int width = 0, height = 0;
        SDL_GetWindowSize(window, &width, &height);
        while (width == 0 || height == 0) {
            SDL_GetWindowSize(window, &width, &height);
            SDL_WaitEvent(nullptr);
        }

        vkDeviceWaitIdle(vulkanDevice.device);

        cleanup();
        createSwapChain(window);
        createImageViews();
        createFramebuffers(renderPass, depthImageView, colorImageView);
    }

    void SwapChain::cleanup() {
        cleanupFramebuffers();

        for (auto imageView : imageViews) {
            vkDestroyImageView(vulkanDevice.device, imageView, nullptr);
        }
        imageViews.clear();

        if (swapChain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(vulkanDevice.device, swapChain, nullptr);
            swapChain = VK_NULL_HANDLE;
        }
    }

    void SwapChain::cleanupFramebuffers() {
        for (auto framebuffer : framebuffers) {
            vkDestroyFramebuffer(vulkanDevice.device, framebuffer, nullptr);
        }
        framebuffers.clear();
    }

    SwapChainSupportDetails SwapChain::querySwapChainSupport(VkPhysicalDevice device) {
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
}
