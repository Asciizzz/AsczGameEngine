#pragma once

#include <memory>
#include <vector>
#include "WindowManager.hpp"
#include "VulkanInstance.hpp"
#include "VulkanDevice.hpp"
#include "SwapChain.hpp"
#include "GraphicsPipeline.hpp"
#include "ShaderManager.hpp"
#include "Buffer.hpp"
#include "Renderer.hpp"
#include "DescriptorManager.hpp"
#include "TextureManager.hpp"
#include "DepthManager.hpp"

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
        std::unique_ptr<WindowManager> windowManager;
        std::unique_ptr<VulkanInstance> vulkanInstance;
        std::unique_ptr<VulkanDevice> vulkanDevice;
        std::unique_ptr<SwapChain> swapChain;
        std::unique_ptr<GraphicsPipeline> graphicsPipeline;
        std::unique_ptr<ShaderManager> shaderManager;
        std::unique_ptr<Buffer> buffer;
        std::unique_ptr<TextureManager> textureManager;
        std::unique_ptr<DepthManager> depthManager;
        std::unique_ptr<DescriptorManager> descriptorManager;
        std::unique_ptr<Renderer> renderer;

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
    };
}
