#pragma once

#include "TinyExt/TinyFS.hpp"

#include "TinyEngine/TinyRData.hpp"

#include "TinyData/TinyCamera.hpp"
#include "TinyEngine/TinyGlobal.hpp"

#include <unordered_set>

class TinyProject {
public:
    TinyProject(const TinyVK::Device* deviceVK);
    
    // UI Selection State - public for easy access from Application
    TinyHandle selectedSceneNodeHandle;
    TinyHandle heldSceneNodeHandle;
    TinyHandle selectedFNodeHandle;
    TinyHandle autoExpandFolderHandle;

    // Delete copy
    TinyProject(const TinyProject&) = delete;
    TinyProject& operator=(const TinyProject&) = delete;
    // No move semantics, where tf would you even want to move it to?

    // Return the scene handle in the registry
    TinyHandle addSceneFromModel(const TinyModel& model); // Returns scene handle - much simpler now!

    TinyCamera* getCamera() const { return tinyCamera.get(); }
    TinyGlobal* getGlobal() const { return tinyGlobal.get(); }
    VkDescriptorSetLayout getGlbDescSetLayout() const { return tinyGlobal->getDescLayout(); }
    VkDescriptorSet getGlbDescSet(uint32_t idx) const { return tinyGlobal->getDescSet(idx); }

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
    
    // Context menu callbacks - set these to handle context menu actions
    std::function<void(TinyHandle)> onAddChildNode = nullptr;
    std::function<void(TinyHandle)> onDeleteNode = nullptr;
    
    // File explorer context menu callbacks
    std::function<void(TinyHandle)> onAddFolder = nullptr;      // parentFolder (or invalid for root)
    std::function<void(TinyHandle)> onAddScene = nullptr;       // parentFolder (or invalid for root)
    std::function<void(TinyHandle)> onDeleteFile = nullptr;     // fileHandle

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
        return scene ? scene->rootNode : TinyHandle();
    }

    const TinyRegistry& registryRef() const { return tinyFS->registryRef(); }
    TinyFS& filesystem() { return *tinyFS; }
    const TinyFS& filesystem() const { return *tinyFS; }

    // Node expansion state management for UI
    void expandNode(TinyHandle nodeHandle) { expandedNodes.insert(nodeHandle); }
    void collapseNode(TinyHandle nodeHandle) { expandedNodes.erase(nodeHandle); }
    bool isNodeExpanded(TinyHandle nodeHandle) const { return expandedNodes.count(nodeHandle) > 0; }
    void expandParentChain(TinyHandle nodeHandle); // Expand all parents up to root

private:
    const TinyVK::Device* deviceVK;

    UniquePtr<TinyGlobal> tinyGlobal;
    UniquePtr<TinyCamera> tinyCamera;

    UniquePtr<TinyFS> tinyFS;

    TinyHandle activeSceneHandle; // Handle to the active scene in registry

    // UI state: track expanded nodes in the hierarchy
    std::unordered_set<TinyHandle> expandedNodes;

    TinyHandle defaultMaterialHandle;
    TinyHandle defaultTextureHandle;

    TinyVK::DescLayout matDescLayout;
    TinyVK::DescPool matDescPool;
    TinyVK::DescSet matDescSet;
};