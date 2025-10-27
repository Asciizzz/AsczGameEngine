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
#include <unordered_set>
#include <vector>
#include <filesystem>

// Unified selection handle for both scene nodes and file system nodes
struct SelectHandle {
    enum class Type { Scene, File };
    
    TinyHandle handle;
    Type type;
    
    SelectHandle() : handle(), type(Type::Scene) {}
    SelectHandle(TinyHandle h, Type t) : handle(h), type(t) {}
    
    bool valid() const { return handle.valid(); }
    bool isScene() const { return type == Type::Scene; }
    bool isFile() const { return type == Type::File; }
    
    void clear() { handle = TinyHandle(); }
    
    bool operator==(const SelectHandle& other) const {
        return handle == other.handle && type == other.type;
    }
    bool operator!=(const SelectHandle& other) const {
        return !(*this == other);
    }
};

// File dialog state
struct FileDialog {
    bool isOpen = false;
    bool justOpened = false;  // Flag to track when we need to call ImGui::OpenPopup
    bool shouldClose = false; // Flag to handle delayed closing
    std::filesystem::path currentPath;
    std::vector<std::filesystem::directory_entry> currentFiles;
    std::string selectedFile;
    TinyHandle targetFolder;
    
    void open(const std::filesystem::path& startPath, TinyHandle folder);
    void close();
    void update();
    void refreshFileList();
    bool isModelFile(const std::filesystem::path& path);
};

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
    
    // UI Selection State - unified selection system
    SelectHandle selectedHandle;        // What is currently selected for inspection
    SelectHandle heldHandle;           // What is being dragged (supports drag from file to component fields)
    SelectHandle autoExpandHandle;     // What should be auto-expanded in UI
    
    // UI state: track expanded nodes in the hierarchy and file explorer
    std::unordered_set<TinyHandle> expandedNodes;
    std::unordered_set<TinyHandle> expandedFNodes;
    
    // File dialog state
    FileDialog fileDialog;
    
    // Active scene state (moved from TinyProject for better separation)
    TinyHandle activeSceneHandle; // Handle to the active scene in registry

    // Window metadata
    const char* appTitle;
    uint32_t appWidth;
    uint32_t appHeight;

    // Functions that actually do things
    void initComponents();
    void mainLoop();
    void cleanup();

    void setupImGuiWindows(const TinyChrono& fpsManager, const TinyCamera& camera, bool mouseLocked, float deltaTime);

    void renderInspectorWindow();
    void renderFileSystemInspector();
    void renderSceneNodeInspector();  // Renamed from renderNodeInspector
    void renderComponent(const char* componentName, ImVec4 backgroundColor, ImVec4 borderColor, bool showRemoveButton, std::function<void()> renderContent, std::function<void()> onRemove);

    bool renderHandleField(const char* fieldId, TinyHandle& handle, const char* targetType, const char* dragTooltip, const char* description = nullptr);

    bool checkWindowResize();
    
    // UI State Management Methods (moved from TinyProject)
    
    // Node expansion state management for UI
    void expandNode(TinyHandle nodeHandle) { expandedNodes.insert(nodeHandle); }
    void collapseNode(TinyHandle nodeHandle) { expandedNodes.erase(nodeHandle); }
    bool isNodeExpanded(TinyHandle nodeHandle) const { return expandedNodes.count(nodeHandle) > 0; }
    void expandParentChain(TinyHandle nodeHandle); // Expand all parents up to root
    
    // File node expansion state management for UI
    void expandFNode(TinyHandle fNodeHandle) { expandedFNodes.insert(fNodeHandle); }
    void collapseFNode(TinyHandle fNodeHandle) { expandedFNodes.erase(fNodeHandle); }
    bool isFNodeExpanded(TinyHandle fNodeHandle) const { return expandedFNodes.count(fNodeHandle) > 0; }
    void expandFNodeParentChain(TinyHandle fNodeHandle); // Expand all parents up to root

    // Unified selection system methods
    void selectSceneNode(TinyHandle nodeHandle) { selectedHandle = SelectHandle(nodeHandle, SelectHandle::Type::Scene); }
    void selectFileNode(TinyHandle fileHandle);
    void clearSelection() { selectedHandle.clear(); }
    
    // Held handle methods (for drag operations)
    void holdSceneNode(TinyHandle nodeHandle) { heldHandle = SelectHandle(nodeHandle, SelectHandle::Type::Scene); }
    void holdFileNode(TinyHandle fileHandle) { heldHandle = SelectHandle(fileHandle, SelectHandle::Type::File); }
    void clearHeld() { heldHandle.clear(); }
    
    // Auto-expand handle methods (for UI expansion)
    void setAutoExpandSceneNode(TinyHandle nodeHandle) { autoExpandHandle = SelectHandle(nodeHandle, SelectHandle::Type::Scene); }
    void setAutoExpandFileNode(TinyHandle fileHandle) { autoExpandHandle = SelectHandle(fileHandle, SelectHandle::Type::File); }
    void clearAutoExpand() { autoExpandHandle.clear(); }
    
    // Get currently selected handles (for backward compatibility and convenience)
    TinyHandle getSelectedSceneNode() const { 
        return selectedHandle.isScene() ? selectedHandle.handle : TinyHandle(); 
    }
    TinyHandle getSelectedFileNode() const { 
        return selectedHandle.isFile() ? selectedHandle.handle : TinyHandle(); 
    }
    TinyHandle getHeldSceneNode() const {
        return heldHandle.isScene() ? heldHandle.handle : TinyHandle();
    }
    TinyHandle getHeldFileNode() const {
        return heldHandle.isFile() ? heldHandle.handle : TinyHandle();
    }
    
    // ImGui rendering methods (moved from TinyProject)
    void renderNodeTreeImGui(TinyHandle nodeHandle = TinyHandle(), int depth = 0);
    void renderFileExplorerImGui(TinyHandle nodeHandle = TinyHandle(), int depth = 0);
    void renderFileDialog();
    void loadModelFromPath(const std::string& filePath, TinyHandle targetFolder);
    
    // Active scene management (moved from TinyProject for better separation)
    TinySceneRT* getActiveScene() const {
        return project->fs().rGet<TinySceneRT>(activeSceneHandle); 
    }
    TinyHandle getActiveSceneHandle() const { return activeSceneHandle; }
    bool setActiveScene(TinyHandle sceneHandle);
    TinyHandle activeSceneRootHandle() const { 
        TinySceneRT* scene = getActiveScene();
        return scene ? scene->rootHandle() : TinyHandle();
    }
};
