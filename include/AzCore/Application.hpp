#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include "AzCore/AzCore.hpp"
#include "AzVulk/AzVulk.hpp"
#include "Az3D/Az3D.hpp"

#include "AzBeta/AzBeta.hpp"

#include "AzGame/Grass.hpp"
#include "AzGame/World.hpp"


class Application {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

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
    UniquePtr<AzVulk::Instance> vkInstance;
    UniquePtr<AzVulk::Device> vkDevice;
    UniquePtr<AzVulk::SwapChain> swapChain;

    // Render pass - shared between pipelines
    UniquePtr<AzVulk::RenderPass> mainRenderPass;
    
    UniquePtr<AzVulk::GraphicsPipeline> opaquePipeline;
    UniquePtr<AzVulk::GraphicsPipeline> skyPipeline; // Beta, very inefficient, yet really amazing

    // More Vulkan ceremony
    UniquePtr<AzVulk::DepthManager> depthManager;
    UniquePtr<AzVulk::MSAAManager> msaaManager;
    UniquePtr<AzVulk::Renderer> renderer;
    
    // Global UBO Data
    UniquePtr<Az3D::GlobalUBOManager> globalUBOManager;

    // Az3D resource management and model manager
    UniquePtr<Az3D::ResourceManager> resourceManager;

    // Some cool game element demos
    UniquePtr<AzGame::Grass> grassSystem;
    UniquePtr<AzGame::World> newWorld;

    UniquePtr<AzBeta::ParticleManager> particleManager;

    // Window metadata
    const char* appTitle;
    uint32_t appWidth;
    uint32_t appHeight;

    // Functions that actually do things
    void initVulkan();
    void mainLoop();
    void cleanup();

    bool checkWindowResize();
};
