#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include "AzCore/AzCore.hpp"
#include "AzVulk/AzVulk.hpp"
#include "Az3D/Az3D.hpp"
#include "AzGame/Grass.hpp"

#include "AzBeta/AzBeta.hpp"

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
    std::unique_ptr<AzCore::WindowManager> windowManager;
    std::unique_ptr<AzCore::FpsManager> fpsManager;
    std::unique_ptr<Az3D::Camera> camera;

    // Vulkan: OpenGL's ambitious cousin
    std::unique_ptr<AzVulk::Instance> vulkanInstance;
    std::unique_ptr<AzVulk::Device> vulkanDevice;
    std::unique_ptr<AzVulk::SwapChain> swapChain;

    // Render pass - shared between pipelines
    std::unique_ptr<AzVulk::RenderPass> mainRenderPass;
    
    std::unique_ptr<AzVulk::RasterPipeline> opaquePipeline;
    std::unique_ptr<AzVulk::RasterPipeline> transparentPipeline;

    // More Vulkan ceremony
    std::unique_ptr<AzVulk::ShaderManager> shaderManager;
    std::unique_ptr<AzVulk::Buffer> buffer;
    std::unique_ptr<AzVulk::DepthManager> depthManager;
    std::unique_ptr<AzVulk::MSAAManager> msaaManager;
    std::unique_ptr<AzVulk::DescriptorManager> descriptorManager;
    std::unique_ptr<AzVulk::Renderer> renderer;
    
    // Az3D resource management and model manager
    std::unique_ptr<Az3D::ResourceManager> resourceManager;
    std::unique_ptr<Az3D::ModelManager> modelManager;
    // World construction using instances
    std::vector<Az3D::ModelInstance> worldInstances;
    
    // AzGame systems
    std::unique_ptr<AzGame::Grass> grassSystem;
    
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
};
