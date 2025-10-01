#pragma once

#include <SDL2/SDL.h>
#include "TinyVK/FrameBuffer.hpp"

namespace TinyVK {
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class SwapChain {
public:
    SwapChain(const DeviceVK* deviceVK, VkSurfaceKHR surface, SDL_Window* window);
    ~SwapChain();
    void cleanup();

    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;

    void createFramebuffers(VkRenderPass renderPass, VkImageView depthImageView);
    void recreateFramebuffers(SDL_Window* window, VkRenderPass renderPass, VkImageView depthImageView);

    const DeviceVK* deviceVK;
    VkSurfaceKHR surface;

    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    VkSwapchainKHR get() const { return swapChain; }

    std::vector<VkImage> images;
    VkFormat imageFormat;
    VkExtent2D extent;
    std::vector<VkImageView> imageViews;

    UniquePtrVec<FrameBuffer> framebuffers;

    // Helper methods 
    void createSwapChain(SDL_Window* window);
    void createImageViews();

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice lDevice);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window);
};

}
