#pragma once

// ============================================================================
// VULKAN BACKEND FOR TINY UI
// ============================================================================
// This file provides a Vulkan implementation of IUIBackend.
// Include this ONLY in projects that use Vulkan.
//
// Usage:
//   #include "tinySystem/tinyUI.hpp"
//   #include "tinySystem/tinyUI_Vulkan.hpp"
//
//   auto* backend = new tinyUI::UIBackend_Vulkan();
//   backend->setVulkanData(vkData);
//   tinyUI::UI::Init(backend, window);
// ============================================================================

#include "tinyUI.hpp"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"
#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#include <stdexcept>

namespace tinyUI {

// Vulkan-specific initialization data
struct VulkanBackendData {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queueFamily = 0;
    VkQueue queue = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;  // Render pass for ImGui overlay
    uint32_t minImageCount = 2;
    uint32_t imageCount = 2;
    
    // Optional: For dynamic pipeline recreation
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
};

class UIBackend_Vulkan : public IUIBackend {
public:
    UIBackend_Vulkan() : m_needsRebuild(false), m_descriptorPool(VK_NULL_HANDLE), m_window(nullptr) {}
    
    ~UIBackend_Vulkan() override {
        cleanup();
    }
    
    // Set Vulkan data before calling Init
    void setVulkanData(const VulkanBackendData& data) {
        m_data = data;
        if (m_descriptorPool == VK_NULL_HANDLE) {
            createDescriptorPool();
        }
    }
    
    void init(const BackendInitInfo& info) override {
        m_window = static_cast<SDL_Window*>(info.windowHandle);
        
        if (m_descriptorPool == VK_NULL_HANDLE) {
            throw std::runtime_error("UIBackend_Vulkan: Must call setVulkanData() before init()!");
        }
        
        // Initialize SDL2 backend
        ImGui_ImplSDL2_InitForVulkan(m_window);
        
        // Initialize Vulkan backend
        ImGui_ImplVulkan_InitInfo vkInitInfo = {};
        vkInitInfo.ApiVersion = VK_API_VERSION_1_0;
        vkInitInfo.Instance = m_data.instance;
        vkInitInfo.PhysicalDevice = m_data.physicalDevice;
        vkInitInfo.Device = m_data.device;
        vkInitInfo.QueueFamily = m_data.queueFamily;
        vkInitInfo.Queue = m_data.queue;
        vkInitInfo.PipelineCache = VK_NULL_HANDLE;
        vkInitInfo.DescriptorPool = m_descriptorPool;
        vkInitInfo.MinImageCount = m_data.minImageCount;
        vkInitInfo.ImageCount = m_data.imageCount;
        vkInitInfo.Allocator = nullptr;
        vkInitInfo.CheckVkResultFn = checkVkResult;
        vkInitInfo.PipelineInfoMain.RenderPass = m_data.renderPass;
        vkInitInfo.PipelineInfoMain.Subpass = 0;
        vkInitInfo.PipelineInfoMain.MSAASamples = m_data.msaaSamples;
        
        ImGui_ImplVulkan_Init(&vkInitInfo);
        
        // Note: Fonts are now automatically uploaded by ImGui backend
    }
    
    void newFrame() override {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
    }
    
    void renderDrawData(ImDrawData* drawData) override {
        // Note: This assumes you're calling this within a render pass
        // The command buffer should be passed externally or managed differently
        // For now, we'll just prepare the draw data
        if (drawData && m_currentCommandBuffer) {
            ImGui_ImplVulkan_RenderDrawData(drawData, m_currentCommandBuffer);
        }
    }
    
    void shutdown() override {
        cleanup();
    }
    
    void onResize(uint32_t width, uint32_t height) override {
        (void)width;
        (void)height;
        m_needsRebuild = true;
    }
    
    const char* getName() const override {
        return "Vulkan";
    }
    
    // ========================================
    // Vulkan-Specific Methods
    // ========================================
    
    // Set the command buffer for the current frame
    void setCommandBuffer(VkCommandBuffer cmd) {
        m_currentCommandBuffer = cmd;
    }
    
    // Update render pass (for window resize)
    void updateRenderPass(VkRenderPass newRenderPass) {
        if (m_data.renderPass != newRenderPass) {
            m_data.renderPass = newRenderPass;
            m_needsRebuild = true;
        }
    }
    
    // Rebuild ImGui Vulkan pipeline if needed
    void rebuildIfNeeded() {
        if (m_needsRebuild) {
            vkDeviceWaitIdle(m_data.device);
            ImGui_ImplVulkan_Shutdown();
            
            ImGui_ImplVulkan_InitInfo vkInitInfo = {};
            vkInitInfo.ApiVersion = VK_API_VERSION_1_0;
            vkInitInfo.Instance = m_data.instance;
            vkInitInfo.PhysicalDevice = m_data.physicalDevice;
            vkInitInfo.Device = m_data.device;
            vkInitInfo.QueueFamily = m_data.queueFamily;
            vkInitInfo.Queue = m_data.queue;
            vkInitInfo.PipelineCache = VK_NULL_HANDLE;
            vkInitInfo.DescriptorPool = m_descriptorPool;
            vkInitInfo.MinImageCount = m_data.minImageCount;
            vkInitInfo.ImageCount = m_data.imageCount;
            vkInitInfo.Allocator = nullptr;
            vkInitInfo.CheckVkResultFn = checkVkResult;
            vkInitInfo.UseDynamicRendering = false;
            vkInitInfo.PipelineInfoMain.RenderPass = m_data.renderPass;
            vkInitInfo.PipelineInfoMain.Subpass = 0;
            vkInitInfo.PipelineInfoMain.MSAASamples = m_data.msaaSamples;
            
            ImGui_ImplVulkan_Init(&vkInitInfo);
            m_needsRebuild = false;
        }
    }
    
    // Process SDL events
    void processEvent(const SDL_Event* event) {
        ImGui_ImplSDL2_ProcessEvent(event);
    }
    
private:
    VulkanBackendData m_data;
    SDL_Window* m_window = nullptr;
    VkCommandBuffer m_currentCommandBuffer = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    bool m_needsRebuild;
    
    void createDescriptorPool() {
        // Create a large descriptor pool for ImGui
        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        
        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1000;
        poolInfo.poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]);
        poolInfo.pPoolSizes = poolSizes;
        
        if (vkCreateDescriptorPool(m_data.device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create ImGui descriptor pool!");
        }
    }
    

    void cleanup() {
        if (m_data.device && m_descriptorPool) {
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplSDL2_Shutdown();
            
            vkDestroyDescriptorPool(m_data.device, m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }
    }
    
    static void checkVkResult(VkResult err) {
        if (err != VK_SUCCESS) {
            throw std::runtime_error("Vulkan error: " + std::to_string(err));
        }
    }
};

} // namespace tinyUI
