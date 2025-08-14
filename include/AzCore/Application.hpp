#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include "AzCore/AzCore.hpp"
#include "AzVulk/AzVulk.hpp"
#include "Az3D/Az3D.hpp"

#include "AzBeta/AzBeta.hpp"
#include "AzGame/Grass.hpp"


class Application {
public:
    Application(const char* title = "Vulkan Application", uint32_t width = 800, uint32_t height = 600);
    ~Application();

    
    // Modern C++ says no sharing
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();

private:
    // Core components
    UniquePtr<AzCore::WindowManager> windowManager;
    UniquePtr<AzCore::FpsManager> fpsManager;
    UniquePtr<Az3D::Camera> camera;

    // Vulkan: OpenGL's ambitious cousin
    UniquePtr<AzVulk::Instance> vulkanInstance;
    UniquePtr<AzVulk::Device> vulkanDevice;
    UniquePtr<AzVulk::SwapChain> swapChain;

    // Render pass - shared between pipelines
    UniquePtr<AzVulk::RenderPass> mainRenderPass;
    
    UniquePtr<AzVulk::RasterPipeline> opaquePipeline;
    UniquePtr<AzVulk::RasterPipeline> transparentPipeline;
    UniquePtr<AzVulk::RasterPipeline> skyPipeline; // Beta, very inefficient, yet really amazing

    // More Vulkan ceremony
    UniquePtr<AzVulk::ShaderManager> shaderManager;
    UniquePtr<AzVulk::Buffer> buffer;
    UniquePtr<AzVulk::DepthManager> depthManager;
    UniquePtr<AzVulk::MSAAManager> msaaManager;
    UniquePtr<AzVulk::DescriptorManager> descriptorManager;
    UniquePtr<AzVulk::Renderer> renderer;
    
    // Az3D resource management and model manager
    UniquePtr<Az3D::ResourceManager> resourceManager;
    UniquePtr<Az3D::ModelManager> modelManager;

    // AzGame systems
    UniquePtr<AzGame::Grass> grassSystem;
    
    // Beta features
    AzBeta::ParticleManager particleManager;

    // Vulkan handles we'll definitely remember to clean up
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    // Window metadata
    const char* appTitle;
    uint32_t appWidth;
    uint32_t appHeight;

    // Functions that actually do things
    void initVulkan();
    void createSurface();
    void mainLoop();
    void cleanup();

    bool checkWindowResize();
};
