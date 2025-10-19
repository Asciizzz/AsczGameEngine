#pragma once

#include "TinySystem/TinyChrono.hpp"
#include "TinySystem/TinyWindow.hpp"
#include "TinySystem/TinyImGui.hpp"

#include "TinyVK/System/Device.hpp"
#include "TinyVK/System/Instance.hpp"

#include "TinyVK/Render/Swapchain.hpp"
#include "TinyVK/Render/RenderPass.hpp"
#include "TinyVK/Render/DepthImage.hpp"

#include "TinyVK/Resource/DataBuffer.hpp"
#include "TinyVK/Resource/Descriptor.hpp"

#include "TinyVK/Render/Renderer.hpp"
#include "TinyVK/Pipeline/Pipeline_include.hpp"

#include "TinyEngine/TinyProject.hpp"

class TinyApp {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    TinyApp(const char* title = "Vulkan TinyApp", uint32_t width = 800, uint32_t height = 600);
    ~TinyApp();

    // Modern C++ says no sharing
    TinyApp(const TinyApp&) = delete;
    TinyApp& operator=(const TinyApp&) = delete;

    void run();

private:
    UniquePtr<TinyWindow> windowManager;
    UniquePtr<TinyChrono> fpsManager;

    UniquePtr<TinyVK::Instance> instanceVK;
    UniquePtr<TinyVK::Device> deviceVK;

    UniquePtr<TinyVK::Renderer> renderer;

    UniquePtr<TinyVK::PipelineManager> pipelineManager;

    UniquePtr<TinyProject> project; // New gigachad system
    UniquePtr<TinyImGui> imguiWrapper; // ImGui integration

    // ImGui UI state
    bool showDebugWindow = true;
    bool showDemoWindow = false;
    bool showEditorSettingsWindow = false;
    bool showInspectorWindow = true;

    // Window metadata
    const char* appTitle;
    uint32_t appWidth;
    uint32_t appHeight;

    // Functions that actually do things
    void initComponents();
    void mainLoop();
    void cleanup();

    void loadAllAssetsRecursively(const std::string& assetsPath);

    void setupImGuiWindows(const TinyChrono& fpsManager, const TinyCamera& camera, bool mouseLocked, float deltaTime);

    void renderInspectorWindow();
    void renderFileSystemInspector();
    void renderSceneNodeInspector();  // Renamed from renderNodeInspector
    void renderComponent(const char* componentName, ImVec4 backgroundColor, ImVec4 borderColor, bool showRemoveButton, std::function<void()> renderContent, std::function<void()> onRemove);

    bool renderHandleField(const char* fieldId, TinyHandle& handle, const char* targetType, const char* dragTooltip, const char* description = nullptr);

    bool checkWindowResize();
};
