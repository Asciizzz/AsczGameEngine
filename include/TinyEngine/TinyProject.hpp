#pragma once

#include "TinyExt/TinyFS.hpp"

#include "TinyEngine/TinyRData.hpp"

#include "TinyData/TinyCamera.hpp"
#include "TinyEngine/TinyGlobal.hpp"

#include <unordered_set>

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

class TinyProject {
public:
    TinyProject(const TinyVK::Device* deviceVK);
    
    // UI Selection State - unified selection system
    SelectHandle selectedHandle;        // What is currently selected for inspection
    SelectHandle heldHandle;           // What is being dragged (supports drag from file to component fields)
    SelectHandle autoExpandHandle;     // What should be auto-expanded in UI

    // Delete copy
    TinyProject(const TinyProject&) = delete;
    TinyProject& operator=(const TinyProject&) = delete;
    // No move semantics, where tf would you even want to move it to?

    // Return the scene handle in the registry
    TinyHandle addSceneFromModel(TinyModel& model, TinyHandle parentFolder = TinyHandle()); // Returns scene handle - much simpler now!

    TinyCamera* getCamera() const { return tinyCamera.get(); }
    TinyGlobal* getGlobal() const { return tinyGlobal.get(); }
    VkDescriptorSetLayout getGlbDescSetLayout() const { return tinyGlobal->getDescLayout(); }
    VkDescriptorSet getGlbDescSet(uint32_t idx) const { return tinyGlobal->getDescSet(idx); }

    void updateGlobal(uint32_t frameIndex) {
        tinyGlobal->update(*tinyCamera, frameIndex);
    }

    /**
     * Adds a scene instance to the active scene.
     *
     * @param sceneHandle Handle to the scene in the registry to instantiate.
     * @param parentNode Handle to the node in active scene to add as child of (optional - uses root if invalid).
     */
    void addSceneInstance(TinyHandle sceneHandle, TinyHandle parentNode = TinyHandle());

    /**
     * Renders an ImGui collapsible tree view with node selection support for the active scene
     */
    void renderNodeTreeImGui(TinyHandle nodeHandle = TinyHandle(), int depth = 0);
    void renderFileExplorerImGui(TinyHandle nodeHandle = TinyHandle(), int depth = 0);
    void renderFileDialog();
    void loadModelFromPath(const std::string& filePath, TinyHandle targetFolder);

    // Active scene access methods
    TinyRScene* getActiveScene() const { return tinyFS->registryRef().get<TinyRScene>(activeSceneHandle); }
    TinyHandle getActiveSceneHandle() const { return activeSceneHandle; }
    
    /**
     * Switch the active scene to a different scene from the registry.
     * @param sceneHandle Handle to a TinyRScene in the registry to make active
     * @return true if successful, false if the handle is invalid or not a scene
     */
    bool setActiveScene(TinyHandle sceneHandle);
    
    // Helper methods for UI compatibility
    TinyHandle getRootNodeHandle() const { 
        TinyRScene* scene = getActiveScene();
        return scene ? scene->rootHandle : TinyHandle();
    }

    const TinyRegistry& registryRef() const { return tinyFS->registryRef(); }
    TinyFS& filesystem() { return *tinyFS; }
    const TinyFS& filesystem() const { return *tinyFS; }

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
    
    // Pending deletion system for safe resource cleanup
    void queueFNodeForDeletion(TinyHandle handle) { pendingFNodeDeletions.push_back(handle); }
    void processPendingDeletions(); // Call after renderer endFrame()
    bool hasPendingDeletions() const { return !pendingFNodeDeletions.empty(); }

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

private:
    const TinyVK::Device* deviceVK;

    UniquePtr<TinyGlobal> tinyGlobal;
    UniquePtr<TinyCamera> tinyCamera;

    UniquePtr<TinyFS> tinyFS;

    TinyHandle activeSceneHandle; // Handle to the active scene in registry

    // UI state: track expanded nodes in the hierarchy and file explorer
    std::unordered_set<TinyHandle> expandedNodes;
    std::unordered_set<TinyHandle> expandedFNodes;
    
    // Pending deletion system to avoid mid-frame Vulkan resource deletion
    std::vector<TinyHandle> pendingFNodeDeletions;

    TinyHandle defaultMaterialHandle;
    TinyHandle defaultTextureHandle;

    TinyVK::DescLayout matDescLayout;
    TinyVK::DescPool matDescPool;
    TinyVK::DescSet matDescSet;
};