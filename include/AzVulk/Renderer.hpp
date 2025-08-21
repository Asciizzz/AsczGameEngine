#pragma once

#include <chrono>

#include "AzVulk/SwapChain.hpp"
#include "AzVulk/DepthManager.hpp"
#include "AzVulk/Pipeline_include.hpp"

#include "Az3D/Az3D.hpp"

namespace AzVulk {
    class DepthManager; // Forward declaration
    class Renderer {
    public:
        Renderer(Device* vkDevice,
                SwapChain* swapChain,
                DepthManager* depthManager,
                Az3D::GlobalUBOManager* globalUBOManager,
                Az3D::ResourceManager* resourceManager);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        // Introduction
        uint32_t beginFrame(GraphicsPipeline& pipeline, Az3D::GlobalUBO& globalUBO);
        // Body
        void drawScene(GraphicsPipeline& pipeline, Az3D::ModelGroup& modelGroup);
        void drawSky(GraphicsPipeline& skyPipeline);
        // Conclusion
        void endFrame(uint32_t imageIndex);
        // Thank's for attending my Ted-Talk

        // Component references
        Device* vkDevice;
        SwapChain* swapChain;
        DepthManager* depthManager;
        Az3D::GlobalUBOManager* globalUBOManager;
        Az3D::ResourceManager* resourceManager;

        // Command recording
        VkCommandPool commandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> commandBuffers;
        
        // Synchronization objects
        std::vector<VkSemaphore> imageAvailableSemaphores; // Per MAX_FRAMES_IN_FLIGHT
        std::vector<VkSemaphore> renderFinishedSemaphores; // Per swapchain image
        std::vector<VkFence> inFlightFences; // Per MAX_FRAMES_IN_FLIGHT
        std::vector<VkFence> imagesInFlight; // Per swapchain image
        
        uint32_t currentFrame = 0;
        bool framebufferResized = false;

        static const int MAX_FRAMES_IN_FLIGHT = 2; // double buffering
        size_t swapchainImageCount = 0;

        void createCommandPool();
        void createCommandBuffers();
        void createSyncObjects();
    };
}