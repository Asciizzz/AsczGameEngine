#pragma once

#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <vector>
#include "AzVulk/Device.hpp"

namespace AzVulk {
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    class SwapChain {
    public:
        SwapChain(const Device& device, VkSurfaceKHR surface, SDL_Window* window);
        ~SwapChain();

        
        SwapChain(const SwapChain&) = delete;
        SwapChain& operator=(const SwapChain&) = delete;

        void recreate(SDL_Window* window, VkRenderPass renderPass, VkImageView depthImageView, VkImageView colorImageView = VK_NULL_HANDLE);
        void createFramebuffers(VkRenderPass renderPass, VkImageView depthImageView, VkImageView colorImageView = VK_NULL_HANDLE);

        
        const Device& vulkanDevice;
        VkSurfaceKHR surface;
        
        VkSwapchainKHR swapChain = VK_NULL_HANDLE;
        std::vector<VkImage> images;
        VkFormat imageFormat;
        VkExtent2D extent;
        std::vector<VkImageView> imageViews;
        std::vector<VkFramebuffer> framebuffers;

        // Helper methods 
        void createSwapChain(SDL_Window* window);
        void createImageViews();
        void cleanup();
        void cleanupFramebuffers();

        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window);
    };
}
