#pragma once

#include <chrono>
#include <glm/glm.hpp>

#include "AzVulk/SwapChain.hpp"
#include "AzVulk/Pipeline_include.hpp"

#include "Az3D/GlobalUBO.hpp"
#include "Az3D/ResourceGroup.hpp"
#include "Az3D/StaticInstance.hpp"

#include "Az3D/RigDemo.hpp"

namespace AzVulk {

    class Renderer {
    public:
        Renderer(Device* vkDevice, SwapChain* swapChain);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        // Introduction
        uint32_t beginFrame(VkRenderPass renderPass, bool hasMSAA);
        
        // Frame tracking
        uint32_t getCurrentFrame() const { return currentFrame; }

        void bindDescSet(const PipelineRaster* pipeline, VkDescriptorSet* sets, uint32_t count) const;

        void drawStaticInstanceGroup(const Az3D::ResourceGroup* resGroup, const Az3D::GlbUBOManager* glbUBO, const PipelineRaster* pipeline, const Az3D::StaticInstanceGroup* instanceGroup) const;
        
        void drawSingleInstance(const Az3D::ResourceGroup* resGroup, const Az3D::GlbUBOManager* glbUBO, const PipelineRaster* pipeline, size_t modelIndex) const;

        void drawDemoRig(const Az3D::ResourceGroup* resGroup, const Az3D::GlbUBOManager* glbUBO, const PipelineRaster* pipeline, Az3D::RigDemo* demo) const;

        void drawSky(const Az3D::GlbUBOManager* glbUBO, const PipelineRaster* skyPipeline) const;

        void endFrame(uint32_t imageIndex);


        // Component references
        Device* vkDevice;
        SwapChain* swapChain;

        // Command recording
        VkCommandPool commandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> cmdBuffers;
        
        // Synchronization objects
        std::vector<VkSemaphore> imageAvailableSemaphores; // Per MAX_FRAMES_IN_FLIGHT
        std::vector<VkSemaphore> renderFinishedSemaphores; // Per swapchain image
        std::vector<VkFence> inFlightFences; // Per MAX_FRAMES_IN_FLIGHT
        std::vector<VkFence> imagesInFlight; // Per swapchain image
        
        uint32_t currentFrame = 0;
        bool framebufferResized = false;

        static const int MAX_FRAMES_IN_FLIGHT = 2; // double buffering
        size_t swapchainImageCount = 0;

        void createCommandBuffers();
        void createSyncObjects();
    };
}