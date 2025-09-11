#pragma once

#include <chrono>
#include <glm/glm.hpp>

#include "AzVulk/CmdBuffer.hpp"
#include "AzVulk/SwapChain.hpp"
#include "AzVulk/Pipeline_include.hpp"
#include "AzVulk/PostProcess.hpp"
#include "AzVulk/DepthManager.hpp"
#include <memory>

#include "Az3D/GlobalUBO.hpp"
#include "Az3D/ResourceGroup.hpp"
#include "Az3D/StaticInstance.hpp"

#include "Az3D/RigDemo.hpp"

namespace AzVulk {

    class Renderer {
    public:
        Renderer(Device* vkDevice, SwapChain* swapChain, DepthManager* depthManager);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        // Introduction
        uint32_t beginFrame(VkRenderPass renderPass);
        
        // Frame tracking
        uint32_t getCurrentFrame() const { return currentFrame; }

        void drawStaticInstanceGroup(const Az3D::ResourceGroup* resGroup, const Az3D::GlbUBOManager* glbUBO, const PipelineRaster* pipeline, const Az3D::StaticInstanceGroup* instanceGroup) const;
        
        void drawSingleInstance(const Az3D::ResourceGroup* resGroup, const Az3D::GlbUBOManager* glbUBO, const PipelineRaster* pipeline, size_t modelIndex) const;

        void drawDemoRig(const Az3D::ResourceGroup* resGroup, const Az3D::GlbUBOManager* glbUBO, const PipelineRaster* pipeline, const Az3D::RigDemo& demo) const;

        void drawSky(const Az3D::GlbUBOManager* glbUBO, const PipelineRaster* skyPipeline) const;

        void endFrame(uint32_t imageIndex);

        // Post-processing methods
        void initializePostProcessing();
        void addPostProcessEffect(const std::string& name, const std::string& computeShaderPath);
        void executePostProcessing(uint32_t imageIndex);

        // Component references
        Device* vkDevice;
        SwapChain* swapChain;
        std::unique_ptr<PostProcess> postProcess;

        // Command recording
        CmdBuffer cmdBuffer;

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
        void copyFinalImageToSwapchain(VkCommandBuffer cmd, uint32_t imageIndex);
    };
}