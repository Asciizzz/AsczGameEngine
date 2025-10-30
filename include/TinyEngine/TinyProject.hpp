#pragma once

#include "tinyExt/tinyFS.hpp"

#include "tinyData/tinyModel.hpp"
#include "tinyData/tinyRT_Scene.hpp"

#include "tinyData/tinyCamera.hpp"
#include "tinyEngine/tinyGlobal.hpp"

class tinyProject {
public:
    static constexpr size_t maxSkeletons = 4096; // This is frighteningly high
    static constexpr size_t maxMaterials = 65536; // Lol

    tinyProject(const tinyVk::Device* deviceVk);
    ~tinyProject();

    tinyProject(const tinyProject&) = delete;
    tinyProject& operator=(const tinyProject&) = delete;
    // No move semantics, where tf would you even want to move it to?

    // Return the scene handle in the registry
    tinyHandle addModel(tinyModel& model, tinyHandle parentFolder = tinyHandle()); // Returns scene handle - much simpler now!
    
    void addSceneInstance(tinyHandle fromHandle, tinyHandle toHandle, tinyHandle parentHandle = tinyHandle());

    // Descriptor accessors

    VkDescriptorSetLayout descSLayout_Global() const { return global_->getDescLayout(); }
    VkDescriptorSetLayout descSLayout_Material() const { return matDescLayout; }

    VkDescriptorSetLayout getSkinDescSetLayout() const { return skinDescLayout.get(); }
    VkDescriptorSet getDummySkinDescSet() const { return dummySkinDescSet.get(); }

    VkDescriptorSet getDefaultMatDescSet() const { return defaultMaterialVk.descSet(); }

    // Global UBO update

    void updateGlobal(uint32_t frameIndex) {
        global_->update(*camera_, frameIndex);
    }


    // Filesystem and registry accessors
    tinyFS& fs() { return *fs_; }
    const tinyFS& fs() const { return *fs_; }

    const tinySceneRT::Require& sceneReq() const { return sharedReq; }
    const tinyVk::Device* vkDevice() const { return deviceVk; }

    tinyCamera* camera() const { return camera_.get(); }
    tinyGlobal* global() const { return global_.get(); }

    tinyHandle initialSceneHandle;

private:
    const tinyVk::Device* deviceVk;

    UniquePtr<tinyGlobal> global_;
    UniquePtr<tinyCamera> camera_;

    UniquePtr<tinyFS> fs_;
    void setupFS();

// -------------- Shared resources --------------

    tinyVk::DescSLayout matDescLayout;
    tinyVk::DescPool matDescPool;

    tinyVk::DescSLayout skinDescLayout;
    tinyVk::DescPool skinDescPool;

    tinySceneRT::Require sharedReq;
    void vkCreateResources();

// -------------- Default resources --------------

    tinyTextureVk defaultTextureVk;
    tinyMaterialVk defaultMaterialVk;

    tinyVk::DescSet dummySkinDescSet;
    tinyVk::DataBuffer dummySkinBuffer;

    void vkCreateDefault();
};