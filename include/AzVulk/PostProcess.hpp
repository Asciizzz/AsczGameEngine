#pragma once

#include <memory>
#include <string>
#include <array>

#include "AzVulk/SwapChain.hpp"
#include "AzVulk/Pipeline_compute.hpp"
#include "AzVulk/DepthManager.hpp"
#include "AzVulk/Descriptor.hpp"

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
    UniquePtr<PipelineCompute> pipeline;

    void cleanup(VkDevice device);
};

class PostProcess {
    friend class Renderer;
public:
    PostProcess(Device* deviceVK, SwapChain* swapChain, DepthManager* depthManager);
    ~PostProcess();

    PostProcess(const PostProcess&) = delete;
    PostProcess& operator=(const PostProcess&) = delete;

    // Initialize post-process resources with external render pass
    void initialize(VkRenderPass offscreenRenderPass);
    
    // Add a post-process effect
    void addEffect(const std::string& name, const std::string& computeShaderPath);
    
    // Get offscreen framebuffer for scene rendering
    VkFramebuffer getOffscreenFramebuffer(uint32_t frameIndex) const;
    VkRenderPass getOffscreenRenderPass() const { return offscreenRenderPass; }
    
    // Set the offscreen render pass (called by Renderer)
    void setOffscreenRenderPass(VkRenderPass renderPass) { offscreenRenderPass = renderPass; }
    
    // Execute all post-process effects
    void executeEffects(VkCommandBuffer cmd, uint32_t frameIndex);
    
    // Execute final blit to swapchain
    void executeFinalBlit(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t swapchainImageIndex);
    
    // Get final image view for presenting
    VkImageView getFinalImageView(uint32_t frameIndex) const;
    
    // Recreate resources when swapchain changes
    void recreate();

private:
    static const int MAX_FRAMES_IN_FLIGHT = 2;
    
    Device* deviceVK;
    SwapChain* swapChain;
    DepthManager* depthManager; // For offscreen framebuffer only
    VkSampler sampler = VK_NULL_HANDLE;
    
    // Per-frame ping-pong images
    std::array<PingPongImages, MAX_FRAMES_IN_FLIGHT> pingPongImages;
    
    // Offscreen render pass - now referenced from Renderer, not owned
    VkRenderPass offscreenRenderPass = VK_NULL_HANDLE;
    std::array<VkFramebuffer, MAX_FRAMES_IN_FLIGHT> offscreenFramebuffers{};
    
    // Post-process effects
    UniquePtrVec<PostProcessEffect> effects;

    // Shared descriptor management for all effects
    DescSets descriptorSets;
    
    // Helper methods
    void createPingPongImages();
    void createOffscreenFramebuffers();
    void createSampler();
    void createSharedDescriptors();
    void createFinalBlit();
    
    void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkFormat format,
                              VkImageLayout oldLayout, VkImageLayout newLayout);
    
    void cleanup();
};

} // namespace AzVulk
