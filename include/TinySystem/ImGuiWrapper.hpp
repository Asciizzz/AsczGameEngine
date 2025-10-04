#pragma once

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"

#include "TinyVK/System/Device.hpp"
#include "TinyVK/Render/RenderPass.hpp"
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

    // Update render pass after window resize (recreates internal render pass)
    void updateRenderPass(const TinyVK::Swapchain* swapchain, const TinyVK::DepthManager* depthManager);
    
    // Get the ImGui render pass for external use
    VkRenderPass getRenderPass() const;

    // Demo window for testing
    void showDemoWindow(bool* p_open = nullptr);

private:
    bool m_initialized = false;
    
    // Vulkan context
    const TinyVK::Device* deviceVK = nullptr;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    
    // Owned render pass for ImGui overlay
    UniquePtr<TinyVK::RenderPass> m_renderPass;
    
    void createDescriptorPool();
    void destroyDescriptorPool();
    void createRenderPass(const TinyVK::Swapchain* swapchain, const TinyVK::DepthManager* depthManager);
};