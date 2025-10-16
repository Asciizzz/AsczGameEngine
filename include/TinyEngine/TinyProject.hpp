#pragma once

#include "TinyExt/TinyFS.hpp"

#include "TinyEngine/TinyRData.hpp"

#include "TinyData/TinyCamera.hpp"
#include "TinyEngine/TinyGlobal.hpp"

class TinyProject {
public:
    TinyProject(const TinyVK::Device* deviceVK);

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
     * Renders an ImGui collapsible tree view of the active scene node hierarchy
     */
    void renderNodeTreeImGui(TinyHandle nodeHandle = TinyHandle(), int depth = 0);
    
    /**
     * Renders an ImGui collapsible tree view with node selection support for the active scene
     */
    void renderSelectableNodeTreeImGui(TinyHandle nodeHandle, TinyHandle& selectedNode, int depth = 0);


    /**
     * Playground function for testing - rotates root node by 90 degrees per second
     * @param dTime Delta time in seconds
     */
    void runPlayground(float dTime);

    // Active scene access methods
    TinyRScene* getActiveScene() const { return tinyFS->registryRef().get<TinyRScene>(activeSceneHandle); }
    TinyHandle getActiveSceneHandle() const { return activeSceneHandle; }
    
    // Helper methods for UI compatibility
    TinyHandle getRootNodeHandle() const { 
        TinyRScene* scene = getActiveScene();
        return scene ? scene->rootNode : TinyHandle();
    }

    const TinyRegistry& registryRef() const { return tinyFS->registryRef(); }
    TinyFS& filesystem() { return *tinyFS; }
    const TinyFS& filesystem() const { return *tinyFS; }

private:
    const TinyVK::Device* deviceVK;

    UniquePtr<TinyGlobal> tinyGlobal;
    UniquePtr<TinyCamera> tinyCamera;

    UniquePtr<TinyFS> tinyFS;

    TinyHandle activeSceneHandle; // Handle to the active scene in registry

    TinyHandle defaultMaterialHandle;
    TinyHandle defaultTextureHandle;

    TinyVK::DescLayout matDescLayout;
    TinyVK::DescPool matDescPool;
    TinyVK::DescSet matDescSet;
};