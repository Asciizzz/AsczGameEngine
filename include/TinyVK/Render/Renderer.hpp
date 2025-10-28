#pragma once

#include <chrono>
#include <functional>
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include <vector>

#include "tinyVK/System/CmdBuffer.hpp"
#include "tinyVK/Render/Swapchain.hpp"
#include "tinyVK/Pipeline/Pipeline_include.hpp"
#include "tinyVK/Render/PostProcess.hpp"
#include "tinyVK/Render/DepthImage.hpp"
#include "tinyVK/Render/RenderPass.hpp"
#include "tinyVK/Render/RenderTarget.hpp"

#include "tinyEngine/tinyProject.hpp"

// Forward declarations
class tinyImGui;

namespace tinyVK {

class Renderer {
public:
    Renderer(Device* deviceVK, VkSurfaceKHR surface, SDL_Window* window, uint32_t maxFramesInFlight = 2);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void recreateRenderPasses();

    void handleWindowResize(SDL_Window* window);

    uint32_t beginFrame();
    void endFrame(uint32_t imageIndex, tinyImGui* imguiWrapper = nullptr);

    uint32_t getCurrentFrame() const { return currentFrame; }
    VkCommandBuffer getCurrentCommandBuffer() const;

    // Direct render target access
    RenderTarget* getSwapchainRenderTarget(uint32_t index) { 
        return (index < swapchainRenderTargets.size()) ? &swapchainRenderTargets[index] : nullptr; 
    }
    RenderTarget* getCurrentRenderTarget() const { return currentRenderTarget; }
    
    // Legacy render pass getters (for backward compatibility)
    VkRenderPass getMainRenderPass() const;
    VkRenderPass getOffscreenRenderPass() const;  // Delegates to PostProcess
    
    // Swapchain getters for external access
    Swapchain* getSwapChain() const { return swapchain.get(); }
    VkExtent2D getSwapChainExtent() const;
    uint32_t getSwapChainImageCount() const { return static_cast<uint32_t>(swapchainImageCount); }
    
    // DepthImage getter for external access
    DepthImage* getDepthManager() const { return depthImage.get(); }
    
    // PostProcess getter for external access
    PostProcess* getPostProcess() const { return postProcess.get(); }

    void drawSky(const tinyProject* project, const PipelineRaster* skyPipeline) const;

    void drawScene(tinyProject* project, tinySceneRT* activeScene, const PipelineRaster* plRigged, const PipelineRaster* plStatic, tinyHandle selectedNodeHandle = tinyHandle()) const;

    // Safe resource deletion with Vulkan synchronization
    void processPendingRemovals(tinyProject* project, tinySceneRT* activeScene);

    // Get swapchain framebuffer for external ImGui rendering
    VkFramebuffer getFrameBuffer(uint32_t imageIndex) const;
    
    // Set up ImGui render targets after framebuffers are created
    void setupImGuiRenderTargets(tinyImGui* imguiWrapper);

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
    UniquePtr<DepthImage> depthImage;

    // Direct render target ownership (no manager needed)
    std::vector<RenderTarget> swapchainRenderTargets;
    RenderTarget* currentRenderTarget = nullptr;

    // Properly owned resources for render targets
    UniquePtr<RenderPass> mainRenderPass;
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



private:
};

}