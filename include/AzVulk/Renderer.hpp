#pragma once

#include <chrono>

#include "AzVulk/SwapChain.hpp"
#include "AzVulk/Pipeline_include.hpp"

#include "Az3D/GlobalUBO.hpp"
#include "Az3D/ResourceGroup.hpp"
#include "Az3D/StaticInstance.hpp"
#include "Az3D/RigInstance.hpp"
#include "Az3D/SkeletonDemo.hpp"


namespace AzVulk {
    class DepthManager; // Forward declaration
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

        // Body
        void drawStaticInstanceGroup(const Az3D::ResourceGroup* resGroup, const Az3D::GlbUBOManager* glbUBO, const PipelineRaster* pipeline, const Az3D::StaticInstanceGroup* instanceGroup);
        // void drawRiggedInstanceGroup(const Az3D::ResourceGroup* resGroup, const Az3D::GlbUBOManager* glbUBO, const PipelineRaster* pipeline, const Az3D::RigInstanceGroup* instanceGroup);
        
        void drawDemoRig(const Az3D::ResourceGroup* resGroup, const Az3D::GlbUBOManager* glbUBO, const PipelineRaster* pipeline, Az3D::RigDemo* demo);

        void drawSky(const Az3D::GlbUBOManager* glbUBO, const PipelineRaster* skyPipeline);

        // Conclusion
        void endFrame(uint32_t imageIndex);

        // Thank's for attending my Ted-Talk

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