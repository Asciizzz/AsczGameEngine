#pragma once

#include <memory>
#include <vector>
#include <SDL2/SDL.h>
#include "AzCore/WindowManager.hpp"
#include "AzCore/FpsManager.hpp"
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
#include "MSAAManager.hpp"

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
        std::unique_ptr<VulkanInstance> vulkanInstance;
        std::unique_ptr<VulkanDevice> vulkanDevice;
        std::unique_ptr<SwapChain> swapChain;

        std::unique_ptr<GraphicsPipeline> graphicsPipeline;

        std::unique_ptr<ShaderManager> shaderManager;
        std::unique_ptr<Buffer> buffer;
        std::unique_ptr<TextureManager> textureManager;
        std::unique_ptr<DepthManager> depthManager;
        std::unique_ptr<MSAAManager> msaaManager;
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

    // TESTING GROUND

        // FPS overlay components
        SDL_Window* fpsWindow = nullptr;
        SDL_Renderer* fpsRenderer = nullptr;
        SDL_Texture* fpsTexture = nullptr;
        std::chrono::steady_clock::time_point lastFpsUpdate;

        // FPS overlay methods
        void initFpsOverlay();
        void renderFpsOverlay();
        void cleanupFpsOverlay();
        void drawSimpleNumber(int x, int y, int number);
        void drawDecimalNumber(int x, int y, int tenths); // For drawing numbers like "1.3"
        void drawSingleDigit(int x, int y, char digit); // Helper for drawing individual digits
    };
}
