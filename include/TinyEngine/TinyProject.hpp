#pragma once

#include "tinyExt/tinyFS.hpp"

#include "tinyEngine/tinyRData.hpp" // Soon to be deprecated
#include "tinyData/tinyModel.hpp"
#include "tinyData/tinyRT_Scene.hpp"

#include "tinyData/tinyCamera.hpp"
#include "tinyEngine/tinyGlobal.hpp"

class tinyProject {
public:
    static constexpr size_t maxSkeletons = 4096; // This is frighteningly high

    tinyProject(const tinyVk::Device* deviceVk);
    ~tinyProject();

    tinyProject(const tinyProject&) = delete;
    tinyProject& operator=(const tinyProject&) = delete;
    // No move semantics, where tf would you even want to move it to?

    // Return the scene handle in the registry
    tinyHandle addModel(tinyModel& model, tinyHandle parentFolder = tinyHandle()); // Returns scene handle - much simpler now!

    tinyCamera* getCamera() const { return camera_.get(); }
    tinyGlobal* getGlobal() const { return global_.get(); }

    // Descriptor accessors

    VkDescriptorSetLayout getGlbDescSetLayout() const { return global_->getDescLayout(); }

    VkDescriptorSetLayout getSkinDescSetLayout() const { return skinDescLayout.get(); }
    VkDescriptorSet getDummySkinDescSet() const { return dummySkinDescSet.get(); }
    
    // Get skin descriptor set with automatic fallback to dummy
    VkDescriptorSet skinDescSet(tinySceneRT* scene, tinyHandle nodeHandle) const {
        return scene->nSkeleDescSet(nodeHandle);
    }

    uint32_t skeletonNodeBoneCount(tinySceneRT* scene, tinyHandle nodeHandle) const {
        const tinyRT_SKEL3D* skeleRT = scene->rtComp<tinyNodeRT::SKEL3D>(nodeHandle);
        return skeleRT ? skeleRT->boneCount() : 0;
    }
    
    // Global UBO update

    void updateGlobal(uint32_t frameIndex) {
        global_->update(*camera_, frameIndex);
    }

    void addSceneInstance(tinyHandle fromHandle, tinyHandle toHandle, tinyHandle parentHandle = tinyHandle());


    // Filesystem and registry accessors
    tinyFS& fs() { return *fs_; }
    const tinyFS& fs() const { return *fs_; }

    const tinySceneRT::Require& sceneReq() const { return sharedReq; }
    const tinyVk::Device* vkDevice() const { return deviceVk; }

    tinyHandle initialSceneHandle;

private:
    const tinyVk::Device* deviceVk;

    UniquePtr<tinyGlobal> global_;
    UniquePtr<tinyCamera> camera_;

    UniquePtr<tinyFS> fs_;

    tinyHandle defaultMaterialHandle;
    tinyHandle defaultTextureHandle;

    tinyVk::DescLayout matDescLayout;
    tinyVk::DescPool matDescPool;
    tinyVk::DescSet matDescSet;

    tinyVk::DescLayout skinDescLayout;
    tinyVk::DescPool skinDescPool;
    
    // Dummy skin descriptor set for rigged meshes without skeleton
    tinyVk::DescSet dummySkinDescSet;
    tinyVk::DataBuffer dummySkinBuffer;

    tinySceneRT::Require sharedReq;
    void vkCreateSceneResources();
    void createDummySkinDescriptorSet();
};