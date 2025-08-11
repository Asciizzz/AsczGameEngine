#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <chrono>
#include "AzVulk/Device.hpp"
#include "AzVulk/SwapChain.hpp"
#include "AzVulk/RasterPipeline.hpp"
#include "AzVulk/Buffer.hpp"
#include "AzVulk/DescriptorManager.hpp"
#include "Az3D/Az3D.hpp"

namespace AzVulk {
    class DepthManager; // Forward declaration
    class Renderer {
    public:
        Renderer(const Device& device, SwapChain& swapChain, Buffer& buffer,
                DescriptorManager& descriptorManager,
                Az3D::ResourceManager& resourceManager);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        // Introduction
        uint32_t beginFrame(RasterPipeline& pipeline, Az3D::Camera& camera);
        // Body
        void drawScene(RasterPipeline& pipeline, Az3D::ModelGroup& modelGroup);
        void drawSky(RasterPipeline& skyPipeline);
        // Conclusion
        void endFrame(uint32_t imageIndex);
        // Thank's for attending my Ted-Talk

        // Component references
        const Device& vulkanDevice;
        SwapChain& swapChain;
        Buffer& buffer;
        DescriptorManager& descriptorManager;
        Az3D::ResourceManager& resourceManager;

        // Command recording
        VkCommandPool commandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> commandBuffers;
        
        // Synchronization objects
        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;
        
        uint32_t currentFrame = 0;
        bool framebufferResized = false;

        static const int MAX_FRAMES_IN_FLIGHT = 2; // double buffering

        // OIT (Order-Independent Transparency) render targets
        VkImage oitAccumImage = VK_NULL_HANDLE;
        VkDeviceMemory oitAccumImageMemory = VK_NULL_HANDLE;
        VkImageView oitAccumImageView = VK_NULL_HANDLE;
        
        VkImage oitRevealImage = VK_NULL_HANDLE;
        VkDeviceMemory oitRevealImageMemory = VK_NULL_HANDLE;
        VkImageView oitRevealImageView = VK_NULL_HANDLE;
        
        VkFramebuffer oitFramebuffer = VK_NULL_HANDLE;
        VkRenderPass oitRenderPass = VK_NULL_HANDLE;

        void createCommandPool();
        void createCommandBuffers();
        void createSyncObjects();
        
        // OIT methods
        void createOITRenderTargets();
        void createOITRenderPass();
        void createOITFramebuffer(VkImageView depthImageView);
        void cleanupOIT();
    };
}