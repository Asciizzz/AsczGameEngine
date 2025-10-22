#pragma once

#include "TinyExt/TinyFS.hpp"

#include "TinyEngine/TinyRData.hpp" // Soon to be deprecated
#include "TinyData/TinyModel.hpp"
#include "TinyData/TinyScene.hpp"

#include "TinyData/TinyCamera.hpp"
#include "TinyEngine/TinyGlobal.hpp"

class TinyProject {
public:
    static constexpr size_t maxSkeletons = 1024;

    TinyProject(const TinyVK::Device* deviceVK);
    ~TinyProject();

    // Delete copy
    TinyProject(const TinyProject&) = delete;
    TinyProject& operator=(const TinyProject&) = delete;
    // No move semantics, where tf would you even want to move it to?

    // Return the scene handle in the registry
    TinyHandle addModel(TinyModel& model, TinyHandle parentFolder = TinyHandle()); // Returns scene handle - much simpler now!

    TinyCamera* getCamera() const { return tinyCamera.get(); }
    TinyGlobal* getGlobal() const { return tinyGlobal.get(); }

    // Descriptor accessors

    VkDescriptorSetLayout getGlbDescSetLayout() const { return tinyGlobal->getDescLayout(); }
    VkDescriptorSet getGlbDescSet(uint32_t idx) const { return tinyGlobal->getDescSet(idx); }
    
    VkDescriptorSetLayout getSkinDescSetLayout() const { return skinDescLayout.get(); }
    VkDescriptorSet getDummySkinDescSet() const { return dummySkinDescSet.get(); }
    
    // Get skin descriptor set with automatic fallback to dummy
    VkDescriptorSet skinDescSet(TinyScene* scene, TinyHandle nodeHandle) const {
        return scene->getNodeSkeletonDescSet(nodeHandle);
    }
    
    // Global UBO update

    void updateGlobal(uint32_t frameIndex) {
        tinyGlobal->update(*tinyCamera, frameIndex);
    }

    /**
     * Adds a scene instance from one scene to another scene.
     *
     * @param fromHandle Handle to the source scene in the registry to instantiate.
     * @param toHandle Handle to the target scene to add the instance to.
     * @param parentHandle Handle to the node in target scene to add as child of (optional - uses root if invalid).
     */
    void addSceneInstance(TinyHandle fromHandle, TinyHandle toHandle, TinyHandle parentHandle = TinyHandle());

    // Filesystem and registry accessors
    TinyFS& fs() { return *tinyFS; }
    const TinyFS& fs() const { return *tinyFS; }

    const TinySceneReq& sceneReq() const { return sharedReq; }

    const TinyVK::Device* vkDevice() const { return deviceVK; }

    // Get the initial scene handle (for TinyApp to set as active scene)
    TinyHandle getInitialSceneHandle() const { return initialSceneHandle; }

private:
    const TinyVK::Device* deviceVK;

    UniquePtr<TinyGlobal> tinyGlobal;
    UniquePtr<TinyCamera> tinyCamera;

    UniquePtr<TinyFS> tinyFS;

    TinyHandle initialSceneHandle; // Handle to the initial "Main Scene" created during construction
    TinyHandle defaultMaterialHandle;
    TinyHandle defaultTextureHandle;

    TinyVK::DescLayout matDescLayout;
    TinyVK::DescPool matDescPool;
    TinyVK::DescSet matDescSet;

    TinyVK::DescLayout skinDescLayout;
    TinyVK::DescPool skinDescPool;
    
    // Dummy skin descriptor set for rigged meshes without skeleton
    TinyVK::DescSet dummySkinDescSet;
    TinyVK::DataBuffer dummySkinBuffer;

    TinySceneReq sharedReq;
    void vkCreateSceneResources();
    void createDummySkinDescriptorSet();
};