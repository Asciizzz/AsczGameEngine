#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <memory>
#include <vector>
#include <SDL2/SDL.h>
#include "AzCore/WindowManager.hpp"
#include "AzCore/FpsManager.hpp"
#include "AzVulk/VulkanInstance.hpp"
#include "AzVulk/VulkanDevice.hpp"
#include "AzVulk/SwapChain.hpp"
#include "AzVulk/GraphicsPipeline.hpp"
#include "AzVulk/ShaderManager.hpp"
#include "AzVulk/Buffer.hpp"
#include "AzVulk/Renderer.hpp"
#include "AzVulk/DescriptorManager.hpp"
#include "AzVulk/DepthManager.hpp"
#include "AzVulk/MSAAManager.hpp"
#include "Az3D/Az3D.hpp"

namespace AzVulk {
    class Application {
    public:
        Application(const char* title = "Vulkan Application", uint32_t width = 800, uint32_t height = 600);
        ~Application();

        // Delete copy constructor and assignment operator
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        void run();

    private:
        // Core components
        std::unique_ptr<AzCore::WindowManager> windowManager;
        std::unique_ptr<AzCore::FpsManager> fpsManager;
        std::unique_ptr<Az3D::Camera> camera;

        std::unique_ptr<VulkanInstance> vulkanInstance;
        std::unique_ptr<VulkanDevice> vulkanDevice;
        std::unique_ptr<SwapChain> swapChain;

        std::vector<
            std::unique_ptr<GraphicsPipeline>
        > graphicsPipelines;
        int pipelineIndex = 0;

        std::unique_ptr<ShaderManager> shaderManager;
        std::unique_ptr<Buffer> buffer;
        std::unique_ptr<DepthManager> depthManager;
        std::unique_ptr<MSAAManager> msaaManager;
        std::unique_ptr<DescriptorManager> descriptorManager;
        std::unique_ptr<Renderer> renderer;
        
        // Az3D resource management
        std::unique_ptr<Az3D::ResourceManager> resourceManager;
        // Az3D scene objects
        std::vector<Az3D::Model> models;

        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        // Application settings
        const char* appTitle;
        uint32_t appWidth;
        uint32_t appHeight;

        void initVulkan();
        void createSurface();
        void mainLoop();
        void cleanup();
        void printResourceSummary();
    };
}
