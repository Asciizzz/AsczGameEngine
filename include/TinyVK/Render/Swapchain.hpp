#pragma once

#include <SDL2/SDL.h>

#include "TinyVK/Render/FrameBuffer.hpp"

#include "TinyVK/Resource/TextureVK.hpp"

namespace TinyVK {
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class Swapchain {
public:
    Swapchain(const Device* deviceVK, VkSurfaceKHR surface, SDL_Window* window);
    ~Swapchain();
    void cleanup();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    void createFrameBuffers(VkRenderPass renderPass, VkImageView depthImageView);
    void recreateFrameBuffers(SDL_Window* window, VkRenderPass renderPass, VkImageView depthImageView);
    
    VkFramebuffer getFrameBuffer(uint32_t index) const;

    const Device* deviceVK;
    VkSurfaceKHR surface;

    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    VkSwapchainKHR get() const { return swapChain; }

    // std::vector<VkImage> images;
    // VkFormat imageFormat;
    // VkExtent2D extent;
    // std::vector<VkImageView> imageViews;

    std::vector<ImageVK> images;

    VkImageView getImageView(uint32_t index) const {
        return (index < images.size()) ? images[index].getView() : VK_NULL_HANDLE;
    }
    VkImage getImage(uint32_t index) const {
        return (index < images.size()) ? images[index].getImage() : VK_NULL_HANDLE;
    }

    VkFormat getImageFormat() const { return images.empty() ? VK_FORMAT_UNDEFINED : images[0].getFormat(); }
    VkExtent2D getExtent() const { return images.empty() ? VkExtent2D{0, 0} : images[0].getExtent2D(); }
    uint32_t getWidth() const { return getExtent().width; }
    uint32_t getHeight() const { return getExtent().height; }
    uint32_t getImageCount() const { return static_cast<uint32_t>(images.size()); }

    bool compareExtent(const VkExtent2D& otherExtent) const {
        return otherExtent.width == getWidth() && otherExtent.height == getHeight();
    }

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
