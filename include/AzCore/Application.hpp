#pragma once

#include "AzCore/FpsManager.hpp"
#include "AzCore/WindowManager.hpp"

#include "AzVulk/DeviceVK.hpp"
#include "AzVulk/Instance.hpp"

#include "AzVulk/SwapChain.hpp"
#include "AzVulk/RenderPass.hpp"
#include "AzVulk/DepthManager.hpp"

#include "AzVulk/DataBuffer.hpp"
#include "AzVulk/Descriptor.hpp"

#include "AzVulk/Renderer.hpp"
#include "AzVulk/Pipeline_include.hpp"

#include "TinyData/TinyCamera.hpp"

#include "TinyEngine/TinyProject.hpp"

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
    UniquePtr<AzVulk::DeviceVK> deviceVK;

    UniquePtr<AzVulk::Renderer> renderer;

    UniquePtr<AzVulk::PipelineManager> pipelineManager;

    UniquePtr<TinyCamera> camera;

    UniquePtr<TinyEngine::GlbUBOManager> glbUBOManager; // Soon to be moved to project
    UniquePtr<TinyEngine::ResourceGroup> resGroup; // Soon to be deprecated
    UniquePtr<TinyProject> project; // New gigachad system

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
