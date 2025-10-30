#pragma once

#include <memory>
#include <string>
#include <array>
#include <fstream>

#include "tinyVk/Render/Swapchain.hpp"
#include "tinyVk/Pipeline/Pipeline_compute.hpp"
#include "tinyVk/Render/DepthImage.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

#include "tinyVk/Render/FrameBuffer.hpp"
#include "tinyVk/Render/RenderTarget.hpp"
#include "tinyVk/Render/RenderPass.hpp"

namespace tinyVk {

// Represents a single ping-pong image pair for one frame
struct PingPongImages {
    ImageVk imageA;
    ImageVk imageB;

    VkImage getImageA() const { return imageA.getImage(); }
    VkImage getImageB() const { return imageB.getImage(); }
    VkImageView getViewA() const { return imageA.getView(); }
    VkImageView getViewB() const { return imageB.getView(); }
    VkDeviceMemory getMemoryA() const { return imageA.getMemory(); }
    VkDeviceMemory getMemoryB() const { return imageB.getMemory(); }

    // No need for custom cleanup with ImageVk
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
    PostProcess(Device* deviceVk, Swapchain* swapchain, DepthImage* depthImage);
    ~PostProcess();

    PostProcess(const PostProcess&) = delete;
    PostProcess& operator=(const PostProcess&) = delete;

    // Initialize post-process resources (creates own render pass)
    void initialize();
    
    // Add a post-process effect
    void addEffect(const std::string& name, const std::string& computeShaderPath);
    void addEffect(const std::string& name, const std::string& computeShaderPath, bool active);
    
    // Load effects from JSON configuration file
    void loadEffectsFromJson(const std::string& configPath);
    
    // Get offscreen render target for scene rendering
    RenderTarget* getOffscreenRenderTarget(uint32_t frameIndex);
    VkRenderPass getOffscreenRenderPass() const { return offscreenRenderPass ? offscreenRenderPass->get() : VK_NULL_HANDLE; }
    
    // Legacy compatibility - get framebuffer directly
    VkFramebuffer getOffscreenFrameBuffer(uint32_t frameIndex) const;
    
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
    
    Device* deviceVk;
    Swapchain* swapchain;
    DepthImage* depthImage; // For offscreen framebuffer only
    
    // Per-frame ping-pong images
    UniquePtrVec<PingPongImages> pingPongImages;

    // Sampler
    UniquePtr<SamplerVk> sampler;

    // Owned offscreen render pass
    UniquePtr<RenderPass> offscreenRenderPass;
    UniquePtrVec<FrameBuffer> offscreenFrameBuffers;
    
    // RenderTarget management
    std::vector<RenderTarget> offscreenRenderTargets;

    OrderedMap<std::string, UniquePtr<PostProcessEffect>> effects;

    // Shared descriptor management for all effects
    UniquePtr<DescSLayout> descSLayout;
    UniquePtr<DescPool> descPool;
    UniquePtrVec<DescSet> descSets;

    // Helper methods
    void createOffscreenRenderPass();
    void createPingPongImages();
    void createOffscreenFrameBuffers();
    void createOffscreenRenderTargets();
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

} // namespace tinyVk
