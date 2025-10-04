#pragma once

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"

#include "TinyVK/Render/RenderPass.hpp"
#include "TinyVK/Render/RenderTarget.hpp"
#include "TinyVK/Render/Swapchain.hpp"
#include "TinyVK/Render/DepthManager.hpp"
#include "TinyVK/Resource/Descriptor.hpp"

#include <SDL2/SDL.h>
#include <functional>

class TinyImGui {
public:
    struct Window {
        std::string name;
        std::function<void()> draw;
        bool* p_open = nullptr; // Optional pointer to control window open/close state
        
        Window(const std::string& windowName, std::function<void()> drawFunc, bool* openPtr = nullptr)
            : name(windowName), draw(drawFunc), p_open(openPtr) {}
    };

    TinyImGui() = default;
    ~TinyImGui() = default;

    // Delete copy semantics
    TinyImGui(const TinyImGui&) = delete;
    TinyImGui& operator=(const TinyImGui&) = delete;

    // Initialize ImGui with SDL2 and Vulkan backends (now creates its own render pass)
    bool init(SDL_Window* window, VkInstance instance, const TinyVK::Device* deviceVK, 
              const TinyVK::Swapchain* swapchain, const TinyVK::DepthManager* depthManager);

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
    TinyVK::DescPool descPool;

    // Owned render pass and render targets for ImGui overlay
    UniquePtr<TinyVK::RenderPass> renderPass;
    std::vector<TinyVK::RenderTarget> renderTargets;
    
    // Font management
    std::vector<std::pair<std::string, ImFont*>> m_loadedFonts;
    
    // Window management
    std::vector<Window> m_windows;
    
    void createDescriptorPool();
    void createRenderPass(const TinyVK::Swapchain* swapchain, const TinyVK::DepthManager* depthManager);
    void createRenderTargets(const TinyVK::Swapchain* swapchain, const TinyVK::DepthManager* depthManager, const std::vector<VkFramebuffer>& framebuffers);
};