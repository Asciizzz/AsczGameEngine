#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <chrono>
#include "AzVulk/Device.hpp"
#include "AzVulk/SwapChain.hpp"
#include "AzVulk/RasterPipeline.hpp"
#include "AzVulk/TransparencyPipeline.hpp"
#include "AzVulk/CompositePipeline.hpp"
#include "AzVulk/Buffer.hpp"
#include "AzVulk/DescriptorManager.hpp"
#include "Az3D/Az3D.hpp"

namespace AzCore {
    class Camera;
}

namespace AzVulk {
    class Renderer {
    public:
        Renderer(const Device& device, SwapChain& swapChain, RasterPipeline& pipeline, 
                Buffer& buffer, DescriptorManager& descriptorManager, Az3D::Camera& camera,
                Az3D::ResourceManager& resourceManager);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        void drawScene( RasterPipeline& pipeline,
                        Az3D::RenderSystem& renderSystem);

        // Component references
        const Device& vulkanDevice;
        SwapChain& swapChain;
        RasterPipeline& graphicsPipeline;
        Buffer& buffer;
        DescriptorManager& descriptorManager;
        Az3D::Camera& camera;
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
        
        std::chrono::high_resolution_clock::time_point startTime;

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
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
        
        // OIT methods
        void createOITRenderTargets();
        void createOITRenderPass();
        void createOITFramebuffer(VkImageView depthImageView);
        void cleanupOIT();
    };
}