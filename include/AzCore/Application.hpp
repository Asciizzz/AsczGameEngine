#pragma once

#include "AzCore/AzCore.hpp"
#include "AzVulk/AzVulk.hpp"
#include "Az3D/Az3D.hpp"

#include "AzBeta/AzBeta.hpp"

#include "AzGame/Grass.hpp"


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
    UniquePtr<AzCore::WindowManager> windowManager;
    UniquePtr<AzCore::FpsManager> fpsManager;

    UniquePtr<AzVulk::Instance> vkInstance;
    UniquePtr<AzVulk::Device> deviceVK;
    UniquePtr<AzVulk::SwapChain> swapChain;

    UniquePtr<AzVulk::RenderPass> mainRenderPass;
    UniquePtr<AzVulk::DepthManager> depthManager;
    UniquePtr<AzVulk::Renderer> renderer;

    UniquePtr<AzVulk::PipelineManager> pipelineManager;

    UniquePtr<Az3D::Camera> camera;
    UniquePtr<Az3D::GlbUBOManager> glbUBOManager;

    UniquePtr<Az3D::ResourceGroup> resGroup;

    // Some cool game element demos
    UniquePtr<AzGame::Grass> grassSystem;
    UniquePtr<AzBeta::ParticleManager> particleManager;
    Az3D::RigDemo rigDemo;

    // Window metadata
    const char* appTitle;
    uint32_t appWidth;
    uint32_t appHeight;

    // Functions that actually do things
    void featuresTestingGround();
    void initComponents();
    void mainLoop();
    void cleanup();

    bool checkWindowResize();
};
