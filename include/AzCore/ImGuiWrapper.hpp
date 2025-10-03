#pragma once

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"

#include "TinyVK/System/Device.hpp"
#include "TinyVK/Render/RenderPass.hpp"

#include <SDL2/SDL.h>

class ImGuiWrapper {
public:
    ImGuiWrapper() = default;
    ~ImGuiWrapper() = default;

    // Delete copy semantics
    ImGuiWrapper(const ImGuiWrapper&) = delete;
    ImGuiWrapper& operator=(const ImGuiWrapper&) = delete;

    // Initialize ImGui with SDL2 and Vulkan backends
    bool init(SDL_Window* window, VkInstance instance, const TinyVK::Device* deviceVK, 
              VkRenderPass renderPass, uint32_t imageCount);

    // Cleanup ImGui
    void cleanup();

    // Start a new ImGui frame
    void newFrame();

    // Render ImGui draw data to command buffer
    void render(VkCommandBuffer commandBuffer);

    // Handle SDL events
    void processEvent(const SDL_Event* event);

    // Update render pass after window resize
    void updateRenderPass(VkRenderPass newRenderPass, uint32_t imageCount);

    // Demo window for testing
    void showDemoWindow(bool* p_open = nullptr);

private:
    bool m_initialized = false;
    
    // Vulkan context
    const TinyVK::Device* m_deviceVK = nullptr;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    
    void createDescriptorPool();
    void destroyDescriptorPool();
};