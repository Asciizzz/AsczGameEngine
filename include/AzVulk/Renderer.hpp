#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <chrono>
#include "AzVulk/VulkanDevice.hpp"
#include "AzVulk/SwapChain.hpp"
#include "AzVulk/GraphicsPipeline.hpp"
#include "AzVulk/Buffer.hpp"
#include "AzVulk/DescriptorManager.hpp"
#include "Az3D/Az3D.hpp"

namespace AzCore {
    class Camera;
}

namespace AzVulk {
    class Renderer {
    public:
        Renderer(const VulkanDevice& device, SwapChain& swapChain, GraphicsPipeline& pipeline, 
                Buffer& buffer, DescriptorManager& descriptorManager, AzCore::Camera& camera);
        ~Renderer();

        // Delete copy constructor and assignment operator
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        void drawFrame();
        void drawFrameWithModels(const std::vector<Az3D::Model>& models, GraphicsPipeline& pipeline);
        
        
        const VulkanDevice& vulkanDevice;
        SwapChain& swapChain;
        GraphicsPipeline& graphicsPipeline;
        Buffer& buffer;
        DescriptorManager& descriptorManager;
        AzCore::Camera& camera;

        VkCommandPool commandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> commandBuffers;
        
        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;
        
        uint32_t currentFrame = 0;
        bool framebufferResized = false;
        std::chrono::high_resolution_clock::time_point startTime;

        static const int MAX_FRAMES_IN_FLIGHT = 2;

        // Helper methods (now public for direct access)
        void createCommandPool();
        void createCommandBuffers();
        void createSyncObjects();
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
        void updateUniformBuffer(uint32_t currentImage);
        void updateUniformBuffer(uint32_t currentImage, const glm::mat4& modelMatrix);
    };
}
