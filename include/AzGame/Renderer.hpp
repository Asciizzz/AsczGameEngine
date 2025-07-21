#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <chrono>
#include "VulkanDevice.hpp"
#include "SwapChain.hpp"
#include "GraphicsPipeline.hpp"
#include "Buffer.hpp"
#include "DescriptorManager.hpp"

namespace AzGame {
    class Renderer {
    public:
        Renderer(const VulkanDevice& device, SwapChain& swapChain, GraphicsPipeline& pipeline, 
                Buffer& buffer, DescriptorManager& descriptorManager);
        ~Renderer();

        // Delete copy constructor and assignment operator
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        void drawFrame();
        bool isFramebufferResized() const { return framebufferResized; }
        void setFramebufferResized(bool resized) { framebufferResized = resized; }
        VkCommandPool getCommandPool() const { return commandPool; }

    private:
        const VulkanDevice& vulkanDevice;
        SwapChain& swapChain;
        GraphicsPipeline& graphicsPipeline;
        Buffer& buffer;
        DescriptorManager& descriptorManager;

        VkCommandPool commandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> commandBuffers;
        
        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;
        
        uint32_t currentFrame = 0;
        bool framebufferResized = false;
        std::chrono::high_resolution_clock::time_point startTime;

        static const int MAX_FRAMES_IN_FLIGHT = 2;
        static const std::vector<Vertex> vertices;
        static const std::vector<uint16_t> indices;

        void createCommandPool();
        void createCommandBuffers();
        void createSyncObjects();
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
        void updateUniformBuffer(uint32_t currentImage);
    };
}
