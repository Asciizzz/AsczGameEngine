#pragma once

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"

#include "tinyVk/Render/RenderPass.hpp"
#include "tinyVk/Render/RenderTarget.hpp"
#include "tinyVk/Render/Swapchain.hpp"
#include "tinyVk/Render/DepthImage.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

#include <SDL2/SDL.h>
#include <functional>

class tinyImGui {
public:
    struct Window {
        std::string name;
        std::function<void()> draw;
        bool* p_open = nullptr; // Optional pointer to control window open/close state
        
        Window(const std::string& windowName, std::function<void()> drawFunc, bool* openPtr = nullptr)
            : name(windowName), draw(drawFunc), p_open(openPtr) {}
    };

    tinyImGui() noexcept = default;
    ~tinyImGui() noexcept = default;

    // Delete copy semantics
    tinyImGui(const tinyImGui&) = delete;
    tinyImGui& operator=(const tinyImGui&) = delete;

    // Initialize ImGui with SDL2 and Vulkan backends (now creates its own render pass)
    bool init(SDL_Window* window, VkInstance instance, const tinyVk::Device* deviceVk, 
              const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage);

    // Cleanup ImGui
    void cleanup();

    // Start a new ImGui frame
    void newFrame();

    // Add a window to the UI system
    void addWindow(const std::string& name, std::function<void()> draw, bool* p_open = nullptr);
    
    // Remove a window by name
    void removeWindow(const std::string& name);
    
    // Clear all windows
    void clearWindows();

    // Render all windows and ImGui draw data to command buffer
    void render(VkCommandBuffer commandBuffer);

    // Handle SDL events
    void processEvent(const SDL_Event* event);

    // Update render pass after window resize (recreates internal render pass and render targets)
    void updateRenderPass(const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage);
    
    // Get the ImGui render pass for external use
    VkRenderPass getRenderPass() const;
    
    // Get render target for specific swapchain image
    tinyVk::RenderTarget* getRenderTarget(uint32_t imageIndex);
    
    // Render to specific swapchain image (requires framebuffers from Renderer)
    void renderToTarget(uint32_t imageIndex, VkCommandBuffer cmd, VkFramebuffer framebuffer);
    
    // Update render targets with framebuffers (called by Renderer after it creates framebuffers)
    void updateRenderTargets(const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage, const std::vector<VkFramebuffer>& framebuffers);

    // Demo window for testing
    void showDemoWindow(bool* p_open = nullptr);

private:
    bool m_initialized = false;

    // Vulkan context
    const tinyVk::Device* deviceVk = nullptr;
    tinyVk::DescPool descPool;

    // Owned render pass and render targets for ImGui overlay
    UniquePtr<tinyVk::RenderPass> renderPass;
    std::vector<tinyVk::RenderTarget> renderTargets;
    
    // Window management
    std::vector<Window> windows;
    
    void createDescriptorPool();
    void createRenderPass(const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage);
    void createRenderTargets(const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage, const std::vector<VkFramebuffer>& framebuffers);
};