#pragma once

#include <memory>
#include <string>
#include <array>
#include <fstream>

#include "AzVulk/SwapChain.hpp"
#include "AzVulk/Pipeline_compute.hpp"
#include "AzVulk/DepthManager.hpp"
#include "AzVulk/Descriptor.hpp"

#include "AzVulk/FrameBuffer.hpp"

namespace AzVulk {

// Represents a single ping-pong image pair for one frame
struct PingPongImages {
    ImageVK imageA;
    ImageVK imageB;

    VkImage getImageA() const { return imageA.getImage(); }
    VkImage getImageB() const { return imageB.getImage(); }
    VkImageView getViewA() const { return imageA.getView(); }
    VkImageView getViewB() const { return imageB.getView(); }
    VkDeviceMemory getMemoryA() const { return imageA.getMemory(); }
    VkDeviceMemory getMemoryB() const { return imageB.getMemory(); }

    // No need for custom cleanup with ImageVK
};

// Configuration for a post-process effect
struct PostProcessEffect {
    std::string computeShaderPath;
    bool active = true;  // Whether this effect is active
    UniquePtr<PipelineCompute> pipeline;

    void cleanup(VkDevice device);
};

class PostProcess {
    friend class Renderer;
public:
    PostProcess(DeviceVK* deviceVK, SwapChain* swapChain, DepthManager* depthManager);
    ~PostProcess();

    PostProcess(const PostProcess&) = delete;
    PostProcess& operator=(const PostProcess&) = delete;

    // Initialize post-process resources with external render pass
    void initialize(VkRenderPass offscreenRenderPass);
    
    // Add a post-process effect
    void addEffect(const std::string& name, const std::string& computeShaderPath);
    void addEffect(const std::string& name, const std::string& computeShaderPath, bool active);
    
    // Load effects from JSON configuration file
    void loadEffectsFromJson(const std::string& configPath);
    
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
    
    DeviceVK* deviceVK;
    SwapChain* swapChain;
    DepthManager* depthManager; // For offscreen framebuffer only
    
    // Per-frame ping-pong images
    UniquePtrVec<PingPongImages> pingPongImages;

    // Sampler
    UniquePtr<SamplerVK> sampler;

    VkRenderPass offscreenRenderPass = VK_NULL_HANDLE;
    // std::array<VkFramebuffer, MAX_FRAMES_IN_FLIGHT> offscreenFramebuffers{};
    UniquePtrVec<FrameBuffer> offscreenFramebuffers;

    OrderedMap<std::string, UniquePtr<PostProcessEffect>> effects;

    // Shared descriptor management for all effects
    UniquePtr<DescLayout> descLayout;
    UniquePtr<DescPool> descPool;
    UniquePtrVec<DescSet> descSets;

    // Helper methods
    void createPingPongImages();
    void createOffscreenFramebuffers();
    void createSampler();
    void createSharedDescriptors();
    void createFinalBlit();
    
    void cleanupRenderResources(); // Images, sampler, descriptors  
    void recreateEffects(); // Recreate stored effects
    
    void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkFormat format,
                              VkImageLayout oldLayout, VkImageLayout newLayout);
    
    // Full cleanup
    void cleanup();
};

} // namespace AzVulk
