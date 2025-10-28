#pragma once

#include "tinySystem/tinyChrono.hpp"
#include "tinySystem/tinyWindow.hpp"
#include "tinySystem/tinyImGui.hpp"

#include "tinyVK/System/Device.hpp"
#include "tinyVK/System/Instance.hpp"

#include "tinyVK/Render/Swapchain.hpp"
#include "tinyVK/Render/RenderPass.hpp"
#include "tinyVK/Render/DepthImage.hpp"

#include "tinyVK/Resource/DataBuffer.hpp"
#include "tinyVK/Resource/Descriptor.hpp"

#include "tinyVK/Render/Renderer.hpp"
#include "tinyVK/Pipeline/Pipeline_include.hpp"

#include "tinyEngine/tinyProject.hpp"
#include <unordered_set>
#include <vector>
#include <filesystem>

// Unified selection handle for both scene nodes and file system nodes
struct SelectHandle {
    enum class Type { Scene, File };
    
    tinyHandle handle;
    Type type;
    
    SelectHandle() : handle(), type(Type::Scene) {}
    SelectHandle(tinyHandle h, Type t) : handle(h), type(t) {}
    
    bool valid() const { return handle.valid(); }
    bool isScene() const { return type == Type::Scene; }
    bool isFile() const { return type == Type::File; }
    
    void clear() { handle = tinyHandle(); }
    
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
    tinyHandle targetFolder;
    
    void open(const std::filesystem::path& startPath, tinyHandle folder);
    void close();
    void update();
    void refreshFileList();
    bool isModelFile(const std::filesystem::path& path);
};

class tinyApp {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    tinyApp(const char* title = "Vulkan tinyApp", uint32_t width = 800, uint32_t height = 600);
    ~tinyApp();

    // Modern C++ says no sharing
    tinyApp(const tinyApp&) = delete;
    tinyApp& operator=(const tinyApp&) = delete;

    void run();

private:
    UniquePtr<tinyWindow> windowManager;
    UniquePtr<tinyChrono> fpsManager;

    UniquePtr<tinyVK::Instance> instanceVK;
    UniquePtr<tinyVK::Device> deviceVK;

    UniquePtr<tinyVK::Renderer> renderer;

    UniquePtr<tinyVK::PipelineManager> pipelineManager;

    UniquePtr<tinyProject> project; // New gigachad system
    UniquePtr<tinyImGui> imguiWrapper; // ImGui integration

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
    std::unordered_set<tinyHandle> expandedNodes;
    std::unordered_set<tinyHandle> expandedFNodes;
    
    // File dialog state
    FileDialog fileDialog;
    
    // Active scene state (moved from tinyProject for better separation)
    tinyHandle activeSceneHandle; // Handle to the active scene in registry

    // Window metadata
    const char* appTitle;
    uint32_t appWidth;
    uint32_t appHeight;

    // Functions that actually do things
    void initComponents();
    void mainLoop();
    void cleanup();

    void setupImGuiWindows(const tinyChrono& fpsManager, const tinyCamera& camera, bool mouseLocked, float deltaTime);

    void renderInspectorWindow();
    void renderFileSystemInspector();
    void renderSceneNodeInspector();  // Renamed from renderNodeInspector
    void renderComponent(const char* componentName, ImVec4 backgroundColor, ImVec4 borderColor, bool showRemoveButton, std::function<void()> renderContent, std::function<void()> onRemove);

    bool renderHandleField(const char* fieldId, tinyHandle& handle, const char* targetType, const char* dragTooltip, const char* description = nullptr);

    bool checkWindowResize();
    
    // UI State Management Methods (moved from tinyProject)
    
    // Node expansion state management for UI
    void expandNode(tinyHandle nodeHandle) { expandedNodes.insert(nodeHandle); }
    void collapseNode(tinyHandle nodeHandle) { expandedNodes.erase(nodeHandle); }
    bool isNodeExpanded(tinyHandle nodeHandle) const { return expandedNodes.count(nodeHandle) > 0; }
    void expandParentChain(tinyHandle nodeHandle); // Expand all parents up to root
    
    // File node expansion state management for UI
    void expandFNode(tinyHandle fNodeHandle) { expandedFNodes.insert(fNodeHandle); }
    void collapseFNode(tinyHandle fNodeHandle) { expandedFNodes.erase(fNodeHandle); }
    bool isFNodeExpanded(tinyHandle fNodeHandle) const { return expandedFNodes.count(fNodeHandle) > 0; }
    void expandFNodeParentChain(tinyHandle fNodeHandle); // Expand all parents up to root

    // Unified selection system methods
    void selectSceneNode(tinyHandle nodeHandle) { selectedHandle = SelectHandle(nodeHandle, SelectHandle::Type::Scene); }
    void selectFileNode(tinyHandle fileHandle);
    void clearSelection() { selectedHandle.clear(); }
    
    // Held handle methods (for drag operations)
    void holdSceneNode(tinyHandle nodeHandle) { heldHandle = SelectHandle(nodeHandle, SelectHandle::Type::Scene); }
    void holdFileNode(tinyHandle fileHandle) { heldHandle = SelectHandle(fileHandle, SelectHandle::Type::File); }
    void clearHeld() { heldHandle.clear(); }
    
    // Auto-expand handle methods (for UI expansion)
    void setAutoExpandSceneNode(tinyHandle nodeHandle) { autoExpandHandle = SelectHandle(nodeHandle, SelectHandle::Type::Scene); }
    void setAutoExpandFileNode(tinyHandle fileHandle) { autoExpandHandle = SelectHandle(fileHandle, SelectHandle::Type::File); }
    void clearAutoExpand() { autoExpandHandle.clear(); }
    
    // Get currently selected handles (for backward compatibility and convenience)
    tinyHandle getSelectedSceneNode() const { 
        return selectedHandle.isScene() ? selectedHandle.handle : tinyHandle(); 
    }
    tinyHandle getSelectedFileNode() const { 
        return selectedHandle.isFile() ? selectedHandle.handle : tinyHandle(); 
    }
    tinyHandle getHeldSceneNode() const {
        return heldHandle.isScene() ? heldHandle.handle : tinyHandle();
    }
    tinyHandle getHeldFileNode() const {
        return heldHandle.isFile() ? heldHandle.handle : tinyHandle();
    }
    
    // ImGui rendering methods (moved from tinyProject)
    void renderNodeTreeImGui(tinyHandle nodeHandle = tinyHandle(), int depth = 0);
    void renderFileExplorerImGui(tinyHandle nodeHandle = tinyHandle(), int depth = 0);
    void renderFileDialog();
    void loadModelFromPath(const std::string& filePath, tinyHandle targetFolder);
    
    // Active scene management (moved from tinyProject for better separation)
    tinySceneRT* getActiveScene() const {
        return project->fs().rGet<tinySceneRT>(activeSceneHandle);
    }
    tinyHandle getActiveSceneHandle() const { return activeSceneHandle; }
    bool setActiveScene(tinyHandle sceneHandle);
    tinyHandle activeSceneRootHandle() const { 
        tinySceneRT* scene = getActiveScene();
        return scene ? scene->rootHandle() : tinyHandle();
    }
};
