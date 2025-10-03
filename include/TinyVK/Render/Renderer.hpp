#pragma once

#include <chrono>
#include <functional>
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include <vector>

#include "TinyVK/System/CmdBuffer.hpp"
#include "TinyVK/Render/Swapchain.hpp"
#include "TinyVK/Pipeline/Pipeline_include.hpp"
#include "TinyVK/Render/PostProcess.hpp"
#include "TinyVK/Render/DepthManager.hpp"
#include "TinyVK/Render/RenderPass.hpp"
#include "TinyVK/Render/RenderTarget.hpp"

#include "TinyEngine/TinyProject.hpp"

namespace TinyVK {

class Renderer {
public:
    Renderer(Device* deviceVK, VkSurfaceKHR surface, SDL_Window* window, uint32_t maxFramesInFlight = 2);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void recreateRenderPasses();

    void handleWindowResize(SDL_Window* window);

    uint32_t beginFrame();
    void endFrame(uint32_t imageIndex);
    void endFrame(uint32_t imageIndex, std::function<void(VkCommandBuffer, VkRenderPass, VkFramebuffer)> imguiRenderFunc);

    uint32_t getCurrentFrame() const { return currentFrame; }
    VkCommandBuffer getCurrentCommandBuffer() const;

    // Render target access
    RenderTarget* getRenderTarget(const std::string& name) { return renderTargets.getTarget(name); }
    RenderTarget* getCurrentRenderTarget() const { return currentRenderTarget; }
    
    // Legacy render pass getters (for backward compatibility)
    VkRenderPass getMainRenderPass() const;
    VkRenderPass getOffscreenRenderPass() const;
    VkRenderPass getImGuiRenderPass() const;
    
    // Swapchain getters for external access
    Swapchain* getSwapChain() const { return swapchain.get(); }
    VkExtent2D getSwapChainExtent() const;
    uint32_t getSwapChainImageCount() const { return static_cast<uint32_t>(swapchainImageCount); }
    
    // DepthManager getter for external access
    DepthManager* getDepthManager() const { return depthManager.get(); }

    void drawSky(const TinyProject* project, const PipelineRaster* skyPipeline) const;

    void drawScene(const TinyProject* project, const PipelineRaster* rPipeline) const;

    // Get swapchain framebuffer for external ImGui rendering
    VkFramebuffer getFrameBuffer(uint32_t imageIndex) const;

    // Post-processing methods
    void addPostProcessEffect(const std::string& name, const std::string& computeShaderPath);
    void loadPostProcessEffectsFromJson(const std::string& configPath);

    bool isResizeNeeded() const { return framebufferResized; }
    void setResizeHandled() { framebufferResized = false; }

private:
    // Component references
    Device* deviceVK;
    
    // Owned components
    UniquePtr<Swapchain> swapchain;
    UniquePtr<DepthManager> depthManager;

    // Render target management
    RenderTargetManager renderTargets;
    RenderTarget* currentRenderTarget = nullptr;

    // Properly owned resources for render targets
    UniquePtr<RenderPass> mainRenderPass;
    UniquePtr<RenderPass> offscreenRenderPass; 
    UniquePtr<RenderPass> imguiRenderPass;
    UniquePtrVec<FrameBuffer> framebuffers;

    UniquePtr<PostProcess> postProcess;

    // Command recording
    CmdBuffer cmdBuffers;

    // Synchronization objects
    std::vector<VkSemaphore> imageAvailableSemaphores; // Per MAX_FRAMES_IN_FLIGHT
    std::vector<VkSemaphore> renderFinishedSemaphores; // Per swapchain image
    std::vector<VkFence> inFlightFences; // Per MAX_FRAMES_IN_FLIGHT
    std::vector<VkFence> imagesInFlight; // Per swapchain image
    
    uint32_t currentFrame = 0;
    bool framebufferResized = false;

    uint32_t maxFramesInFlight = 2; // double buffering
    size_t swapchainImageCount = 0;

    void createCommandBuffers();
    void createSyncObjects();
    void createRenderTargets();
};

}