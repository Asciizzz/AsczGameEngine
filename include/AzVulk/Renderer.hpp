#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <chrono>
#include "AzVulk/Device.hpp"
#include "AzVulk/SwapChain.hpp"
#include "AzVulk/Pipeline.hpp"
#include "AzVulk/Buffer.hpp"
#include "AzVulk/DescriptorManager.hpp"
#include "AzVulk/DepthManager.hpp"

#include "Az3D/Az3D.hpp"

namespace AzVulk {
    class DepthManager; // Forward declaration
    class Renderer {
    public:
        Renderer(const Device& device, SwapChain& swapChain, Buffer& buffer,
                DescriptorManager& descriptorManager,
                Az3D::ResourceManager& resourceManager,
                DepthManager& depthManager);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        // Introduction
        uint32_t beginFrame(Pipeline& pipeline, Az3D::Camera& camera);
        // Body
        void drawScene(Pipeline& pipeline, Az3D::ModelGroup& modelGroup);
        void drawSky(Pipeline& skyPipeline);
        // Conclusion
        void endFrame(uint32_t imageIndex);
        // Thank's for attending my Ted-Talk

        // Component references
        const Device& vulkanDevice;
        SwapChain& swapChain;
        Buffer& buffer;
        DescriptorManager& descriptorManager;
        Az3D::ResourceManager& resourceManager;
        DepthManager& depthManager;

        // Command recording
        VkCommandPool commandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> commandBuffers;
        
        // Synchronization objects
        std::vector<VkFence> imagesInFlight; // Per swapchain image
        std::vector<VkSemaphore> imageAvailableSemaphores; // Per swapchain image
        std::vector<VkSemaphore> renderFinishedSemaphores; // Per swapchain image
        std::vector<VkFence> inFlightFences;
        
        uint32_t currentFrame = 0;
        bool framebufferResized = false;

        static const int MAX_FRAMES_IN_FLIGHT = 2; // double buffering
        size_t swapchainImageCount = 0;

        void createCommandPool();
        void createCommandBuffers();
        void createSyncObjects();
    };
}