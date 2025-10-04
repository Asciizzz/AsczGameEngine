#pragma once

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"

#include "TinyVK/System/Device.hpp"
#include "TinyVK/Render/RenderPass.hpp"
#include "TinyVK/Render/RenderTarget.hpp"
#include "TinyVK/Render/Swapchain.hpp"
#include "TinyVK/Render/DepthManager.hpp"

#include <SDL2/SDL.h>

class ImGuiWrapper {
public:
    ImGuiWrapper() = default;
    ~ImGuiWrapper() = default;

    // Delete copy semantics
    ImGuiWrapper(const ImGuiWrapper&) = delete;
    ImGuiWrapper& operator=(const ImGuiWrapper&) = delete;

    // Initialize ImGui with SDL2 and Vulkan backends (now creates its own render pass)
    bool init(SDL_Window* window, VkInstance instance, const TinyVK::Device* deviceVK, 
              const TinyVK::Swapchain* swapchain, const TinyVK::DepthManager* depthManager);

    // Cleanup ImGui
    void cleanup();

    // Start a new ImGui frame
    void newFrame();

    // Render ImGui draw data to command buffer
    void render(VkCommandBuffer commandBuffer);

    // Handle SDL events
    void processEvent(const SDL_Event* event);

    // Update render pass after window resize (recreates internal render pass and render targets)
    void updateRenderPass(const TinyVK::Swapchain* swapchain, const TinyVK::DepthManager* depthManager);
    
    // Get the ImGui render pass for external use
    VkRenderPass getRenderPass() const;
    
    // Get render target for specific swapchain image
    TinyVK::RenderTarget* getRenderTarget(uint32_t imageIndex);
    
    // Render to specific swapchain image (requires framebuffers from Renderer)
    void renderToTarget(uint32_t imageIndex, VkCommandBuffer cmd, VkFramebuffer framebuffer);
    
    // Update render targets with framebuffers (called by Renderer after it creates framebuffers)
    void updateRenderTargets(const TinyVK::Swapchain* swapchain, const TinyVK::DepthManager* depthManager, const std::vector<VkFramebuffer>& framebuffers);

    // Demo window for testing
    void showDemoWindow(bool* p_open = nullptr);

private:
    bool m_initialized = false;
    
    // Vulkan context
    const TinyVK::Device* deviceVK = nullptr;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    
    // Owned render pass and render targets for ImGui overlay
    UniquePtr<TinyVK::RenderPass> m_renderPass;
    std::vector<TinyVK::RenderTarget> m_renderTargets;
    
    void createDescriptorPool();
    void destroyDescriptorPool();
    void createRenderPass(const TinyVK::Swapchain* swapchain, const TinyVK::DepthManager* depthManager);
    void createRenderTargets(const TinyVK::Swapchain* swapchain, const TinyVK::DepthManager* depthManager, const std::vector<VkFramebuffer>& framebuffers);
};