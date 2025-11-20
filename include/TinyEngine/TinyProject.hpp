#pragma once

#include "tinyFS.hpp"

#include "tinyModel.hpp"
#include "tinyRT_Scene.hpp"

#include "tinyCamera.hpp"
#include "tinyEngine/tinyGlobal.hpp"

class tinyProject {
public:
    static constexpr size_t maxSkeletons = 4096; // This is frighteningly high
    static constexpr size_t maxMaterials = 65536; // Lol
    static constexpr size_t maxMeshes    = 65536; 

    tinyProject(const tinyVk::Device* deviceVk_);
    ~tinyProject();

    tinyProject(const tinyProject&) = delete;
    tinyProject& operator=(const tinyProject&) = delete;
    // No move semantics, where tf would you even want to move it to?

    // Return the scene handle in the registry
    tinyHandle addModel(tinyModel& model, tinyHandle parentFolder = tinyHandle()); // Returns scene handle - much simpler now!
    
    void addSceneInstance(tinyHandle fromHandle, tinyHandle toHandle, tinyHandle parentHandle = tinyHandle());

    // Descriptor accessors

    // Only need the active 3
    VkDescriptorSetLayout descSLayout_Global() const { return global_->getDescLayout(); }

    // Global UBO update

    void updateGlobal(uint32_t frameIndex) {
        global_->update(*camera_, frameIndex);
    }


    // Filesystem and registry accessors
    tinyFS& fs() { return *fs_; }
    const tinyFS& fs() const { return *fs_; }

    tinyRegistry& registry() { return fs_->registry(); }
    const tinyRegistry& registry() const { return fs_->registry(); }

    const tinySharedRes& sharedRes() const { return sharedRes_; }
    const tinyVk::Device* vkDevice() const { return deviceVk_; }

    tinyCamera* camera() const { return camera_.get(); }
    tinyGlobal* global() const { return global_.get(); }

    tinyHandle mainSceneHandle;

    tinySceneRT* scene(tinyHandle& sceneHandle = tinyHandle()) {
        sceneHandle = sceneHandle ? sceneHandle : mainSceneHandle;
        return registry().get<tinySceneRT>(sceneHandle);
    }

private:
    const tinyVk::Device* deviceVk_;

    UniquePtr<tinyGlobal> global_;
    UniquePtr<tinyCamera> camera_;

    UniquePtr<tinyFS> fs_;
    void setupFS();

    template<typename T>
    tinyHandle rAdd(T&& resource) {
        return fs_->registry().emplace<T>(std::forward<T>(resource));
    }

// -------------- Shared resources --------------

    tinySharedRes sharedRes_;
    void vkCreateResources();

    void vkCreateDefault();
};