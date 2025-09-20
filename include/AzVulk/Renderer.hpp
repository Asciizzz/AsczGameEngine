#pragma once

#include <chrono>
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include <vector>

#include "AzVulk/CmdBuffer.hpp"
#include "AzVulk/SwapChain.hpp"
#include "AzVulk/Pipeline_include.hpp"
#include "AzVulk/PostProcess.hpp"
#include "AzVulk/DepthManager.hpp"
#include "AzVulk/RenderPass.hpp"

#include "Az3D/GlobalUBO.hpp"
#include "Az3D/ResourceGroup.hpp"
#include "Az3D/StaticInstance.hpp"

namespace AzVulk {

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

    uint32_t getCurrentFrame() const { return currentFrame; }

    // Render pass getters
    VkRenderPass getMainRenderPass() const;
    VkRenderPass getOffscreenRenderPass() const;
    
    // SwapChain getters for external access
    SwapChain* getSwapChain() const { return swapChain.get(); }
    VkExtent2D getSwapChainExtent() const;
    
    // DepthManager getter for external access
    DepthManager* getDepthManager() const { return depthManager.get(); }

    // Demo functions
    void drawStaticInstanceGroup(const Az3D::ResourceGroup* resGroup, const Az3D::GlbUBOManager* glbUBO, const PipelineRaster* pipeline, const Az3D::StaticInstanceGroup* instanceGroup) const;
    
    void drawSingleInstance(const Az3D::ResourceGroup* resGroup, const Az3D::GlbUBOManager* glbUBO, const PipelineRaster* pipeline, size_t modelIndex) const;

    void drawSky(const Az3D::GlbUBOManager* glbUBO, const PipelineRaster* skyPipeline) const;


    // Post-processing methods
    void addPostProcessEffect(const std::string& name, const std::string& computeShaderPath);
    void loadPostProcessEffectsFromJson(const std::string& configPath);

    bool isResizeNeeded() const { return framebufferResized; }
    void setResizeHandled() { framebufferResized = false; }

private:
    // Component references
    Device* deviceVK;
    
    // Owned components
    UniquePtr<SwapChain> swapChain;
    UniquePtr<DepthManager> depthManager;
    
    // Render passes owned by this renderer
    UniquePtr<RenderPass> mainRenderPass;      // For final presentation
    UniquePtr<RenderPass> offscreenRenderPass; // For scene rendering
    
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
    void createRenderPasses();
};

}