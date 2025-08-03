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

        void createCommandPool();
        void createCommandBuffers();
        void createSyncObjects();
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    };
}