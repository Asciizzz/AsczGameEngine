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
#include "Az3D/RenderingSystem.hpp"

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

        // Delete copy constructor and assignment operator
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        // New render system-based draw function
        void drawFrame(RasterPipeline& pipeline,
                       Az3D::RenderSystem& renderSystem,
                       const std::vector<Az3D::Billboard>& billboards = {});

        // Legacy draw function (deprecated)
        void drawFrame(RasterPipeline& pipeline,
                       const std::vector<Az3D::Model>& models, 
                       const std::vector<Az3D::Billboard>& billboards = {});

        void setupBillboardDescriptors();

        const Device& vulkanDevice;
        SwapChain& swapChain;
        RasterPipeline& graphicsPipeline;
        Buffer& buffer;
        DescriptorManager& descriptorManager;
        DescriptorManager billboardDescriptorManager;  // Integrated billboard descriptor manager
        Az3D::Camera& camera;
        Az3D::ResourceManager& resourceManager;

        VkCommandPool commandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> commandBuffers;
        
        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;
        
        uint32_t currentFrame = 0;
        bool framebufferResized = false;
        std::chrono::high_resolution_clock::time_point startTime;

        static const int MAX_FRAMES_IN_FLIGHT = 2;

    private:
        void createCommandPool();
        void createCommandBuffers();
        void createSyncObjects();
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    };
}
