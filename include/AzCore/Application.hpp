#pragma once

#include "AzCore/FpsManager.hpp"
#include "AzCore/WindowManager.hpp"

#include "TinyVK/Device.hpp"
#include "TinyVK/Instance.hpp"

#include "TinyVK/SwapChain.hpp"
#include "TinyVK/RenderPass.hpp"
#include "TinyVK/DepthManager.hpp"

#include "TinyVK/DataBuffer.hpp"
#include "TinyVK/Descriptor.hpp"

#include "TinyVK/Renderer.hpp"
#include "TinyVK/Pipeline_include.hpp"

#include "TinyData/TinyCamera.hpp"

#include "TinyEngine/TinyProject.hpp"
#include "AzCore/ImGuiWrapper.hpp"

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
    UniquePtr<WindowManager> windowManager;
    UniquePtr<FpsManager> fpsManager;

    UniquePtr<TinyVK::Instance> vkInstance;
    UniquePtr<TinyVK::Device> deviceVK;

    UniquePtr<TinyVK::Renderer> renderer;

    UniquePtr<TinyVK::PipelineManager> pipelineManager;

    UniquePtr<TinyProject> project; // New gigachad system
    UniquePtr<ImGuiWrapper> imguiWrapper; // ImGui integration

    // ImGui UI state
    bool showDebugWindow = true;
    bool showDemoWindow = false;

    // Window metadata
    const char* appTitle;
    uint32_t appWidth;
    uint32_t appHeight;

    // Functions that actually do things
    void featuresTestingGround();
    void initComponents();
    void mainLoop();
    void cleanup();
    void createImGuiUI(const FpsManager& fpsManager, const TinyCamera& camera, bool mouseLocked, float deltaTime);

    bool checkWindowResize();
};
