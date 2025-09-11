#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <string>
#include <array>

#include "AzVulk/Device.hpp"
#include "AzVulk/SwapChain.hpp"
#include "AzVulk/Pipeline_compute.hpp"
#include "AzVulk/DepthManager.hpp"

namespace AzVulk {

// Represents a single ping-pong image pair for one frame
struct PingPongImages {
    VkImage imageA = VK_NULL_HANDLE;
    VkImage imageB = VK_NULL_HANDLE;
    VkDeviceMemory memoryA = VK_NULL_HANDLE;
    VkDeviceMemory memoryB = VK_NULL_HANDLE;
    VkImageView viewA = VK_NULL_HANDLE;
    VkImageView viewB = VK_NULL_HANDLE;
    
    void cleanup(VkDevice device);
};

// Configuration for a post-process effect
struct PostProcessEffect {
    std::string name;
    std::string computeShaderPath;
    std::unique_ptr<PipelineCompute> pipeline;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    
    // Per-frame descriptor sets: [frame][pingpong_direction]
    // pingpong_direction: 0 = A->B, 1 = B->A
    std::vector<std::array<VkDescriptorSet, 2>> descriptorSets;
    
    void cleanup(VkDevice device);
};

class PostProcess {
    friend class Renderer;
public:
    PostProcess(Device* vkDevice, SwapChain* swapChain, DepthManager* depthManager);
    ~PostProcess();

    PostProcess(const PostProcess&) = delete;
    PostProcess& operator=(const PostProcess&) = delete;

    // Initialize post-process resources
    void initialize();
    
    // Add a post-process effect
    void addEffect(const std::string& name, const std::string& computeShaderPath);
    
    // Get offscreen framebuffer for scene rendering
    VkFramebuffer getOffscreenFramebuffer(uint32_t frameIndex) const;
    VkRenderPass getOffscreenRenderPass() const { return offscreenRenderPass; }
    
    // Execute all post-process effects
    void executeEffects(VkCommandBuffer cmd, uint32_t frameIndex);
    
    // Execute final blit to swapchain
    void executeFinalBlit(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t swapchainImageIndex);
    
    // Get final image view for presenting
    VkImageView getFinalImageView(uint32_t frameIndex) const;
    
    // Recreate resources when swapchain changes
    void recreate();
    
    // Helper method for creating image views
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

private:
    static const int MAX_FRAMES_IN_FLIGHT = 2;
    
    Device* vkDevice;
    SwapChain* swapChain;
    DepthManager* depthManager;
    VkSampler sampler = VK_NULL_HANDLE;
    
    // Per-frame ping-pong images
    std::array<PingPongImages, MAX_FRAMES_IN_FLIGHT> pingPongImages;
    
    // Offscreen render pass and framebuffers for scene rendering
    VkRenderPass offscreenRenderPass = VK_NULL_HANDLE;
    std::array<VkFramebuffer, MAX_FRAMES_IN_FLIGHT> offscreenFramebuffers{};
    
    // Note: Depth buffer is managed by DepthManager
    
    // Post-process effects
    std::vector<std::unique_ptr<PostProcessEffect>> effects;
    

    

    
    // Helper methods
    void createPingPongImages();
    void createOffscreenRenderPass();
    void createOffscreenFramebuffers();
    void createDepthResources();
    void createSampler();
    void createFinalBlit();
    
    void createImage(uint32_t width, uint32_t height, VkFormat format, 
                     VkImageTiling tiling, VkImageUsageFlags usage, 
                     VkMemoryPropertyFlags properties, VkImage& image, 
                     VkDeviceMemory& imageMemory);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkFormat findDepthFormat();
    
    void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkFormat format,
                              VkImageLayout oldLayout, VkImageLayout newLayout);
    
    void cleanup();
};

} // namespace AzVulk
