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
    
    // Scene management (now handled via TinyFS folder structure)
    
    // Runtime scene node selection for hierarchy interaction
    TinyHandle selectedSceneNodeHandle;  // Currently active node for Inspector/editing
    TinyHandle heldNodeHandle;           // Temporarily held node for drag operations
    
    // Filesystem node selection (folders and files)
    TinyHandle selectedFNodeHandle;
    
    // Deferred deletion system to handle resource cleanup after frame completion
    std::vector<TinyHandle> pendingDeletions;

    // Window metadata
    const char* appTitle;
    uint32_t appWidth;
    uint32_t appHeight;

    // Functions that actually do things
    void featuresTestingGround();
    void initComponents();
    void mainLoop();
    void cleanup();

    void setupImGuiWindows(const TinyChrono& fpsManager, const TinyCamera& camera, bool mouseLocked, float deltaTime);
    void loadAllAssetsRecursively(const std::string& assetsPath);
    void renderInspectorWindow();
    void renderSceneFolderTree(TinyFS& fs, TinyHandle folderHandle, int depth = 0);
    
    // Inspector helper functions
    void renderFileSystemInspector();
    void renderNodeInspector();
    void renderTransformInspector(const TinyRNode* selectedNode, bool isRootNode);
    void renderComponentsInspector(const TinyRNode* selectedNode);
    void renderNodeDeleteButton(const TinyRNode* selectedNode);
    
    // Component-specific inspectors
    void renderMeshRenderComponent(TinyRNode* selectedNode);
    void renderSkeletonComponent(TinyRNode* selectedNode);
    void renderBoneAttachComponent(TinyRNode* selectedNode);
    void renderAddComponentDropdown(const TinyRNode* selectedNode);
    
    // Drag-drop helper for component fields
    bool renderHandleField(const char* label, TinyHandle& handle, const char* targetType, const char* tooltip = nullptr);
    bool renderEnhancedHandleField(const char* fieldId, TinyHandle& handle, const char* targetType, const char* dragTooltip, const char* description = nullptr);
    
    // Deferred deletion methods
    void queueForDeletion(TinyHandle handle);
    void processPendingDeletions();

    bool checkWindowResize();
};
