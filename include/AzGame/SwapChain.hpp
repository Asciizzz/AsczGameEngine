#pragma once

#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <vector>
#include "VulkanDevice.hpp"

namespace AzGame {
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    class SwapChain {
    public:
        SwapChain(const VulkanDevice& device, VkSurfaceKHR surface, SDL_Window* window);
        ~SwapChain();

        // Delete copy constructor and assignment operator
        SwapChain(const SwapChain&) = delete;
        SwapChain& operator=(const SwapChain&) = delete;

        VkSwapchainKHR getSwapChain() const { return swapChain; }
        VkFormat getImageFormat() const { return swapChainImageFormat; }
        VkExtent2D getExtent() const { return swapChainExtent; }
        const std::vector<VkImage>& getImages() const { return swapChainImages; }
        const std::vector<VkImageView>& getImageViews() const { return swapChainImageViews; }
        const std::vector<VkFramebuffer>& getFramebuffers() const { return swapChainFramebuffers; }
        size_t getImageCount() const { return swapChainImages.size(); }

        void recreate(SDL_Window* window, VkRenderPass renderPass, VkImageView depthImageView);
        void createFramebuffers(VkRenderPass renderPass, VkImageView depthImageView);

    private:
        const VulkanDevice& vulkanDevice;
        VkSurfaceKHR surface;
        
        VkSwapchainKHR swapChain = VK_NULL_HANDLE;
        std::vector<VkImage> swapChainImages;
        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;
        std::vector<VkImageView> swapChainImageViews;
        std::vector<VkFramebuffer> swapChainFramebuffers;

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
