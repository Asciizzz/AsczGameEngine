#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include "AzCore/AzCore.hpp"
#include "AzVulk/AzVulk.hpp"
#include "Az3D/Az3D.hpp"

#include "AzBeta/AzBeta.hpp"

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

    std::unique_ptr<AzVulk::Instance> vulkanInstance;
    std::unique_ptr<AzVulk::Device> vulkanDevice;
    std::unique_ptr<AzVulk::SwapChain> swapChain;

    std::vector<
        std::unique_ptr<AzVulk::RasterPipeline>
    > rasterPipeline;
    int pipelineIndex = 0;

    std::unique_ptr<AzVulk::ShaderManager> shaderManager;
    std::unique_ptr<AzVulk::Buffer> buffer;
    std::unique_ptr<AzVulk::DepthManager> depthManager;
    std::unique_ptr<AzVulk::MSAAManager> msaaManager;
    std::unique_ptr<AzVulk::DescriptorManager> descriptorManager;
    std::unique_ptr<AzVulk::Renderer> renderer;
    
    // Az3D resource management (Mesh - Material - Texture - Light)
    std::unique_ptr<Az3D::ResourceManager> resourceManager;
    // Az3D render system for efficient batching
    std::unique_ptr<Az3D::RenderSystem> renderSystem;
    
    // Beta features
    AzBeta::ParticleManager particleManager;
    AzBeta::Map gameMap;
    
    // Model resource indices for the new rendering system
    size_t mapModelResourceIndex = 0;

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
