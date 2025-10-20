#pragma once

#include "TinyExt/TinyFS.hpp"

#include "TinyEngine/TinyRData.hpp" // Soon to be deprecated
#include "TinyData/TinyModel.hpp"
#include "TinyData/TinyScene.hpp"

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



    // Active scene access methods
    TinyScene* getActiveScene() const { return tinyFS->registryRef().get<TinyScene>(activeSceneHandle); }
    TinyHandle getActiveSceneHandle() const { return activeSceneHandle; }
    
    /**
     * Switch the active scene to a different scene from the registry.
     * @param sceneHandle Handle to a TinyScene in the registry to make active
     * @return true if successful, false if the handle is invalid or not a scene
     */
    bool setActiveScene(TinyHandle sceneHandle);
    
    // Helper methods for UI compatibility
    TinyHandle getRootNodeHandle() const { 
        TinyScene* scene = getActiveScene();
        return scene ? scene->rootHandle : TinyHandle();
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