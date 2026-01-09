#pragma once


#include "tinyModel.hpp"

#include "tinyCamera.hpp"
#include "tinyGlobal.hpp"

#include "ascFS.hpp"

#include "tinyRT/rtScene.hpp"
#include "tinyDrawable.hpp"

class tinyProject {
public:
    tinyProject(const tinyVk::Device* dvk_);
    ~tinyProject();

    tinyProject(const tinyProject&) = delete;
    tinyProject& operator=(const tinyProject&) = delete;
    // No move semantics, where tf would you even want to move it to?

    // Return the scene handle in the registry
    Asc::Handle addModel(tinyModel& model, Asc::Handle parentFolder = Asc::Handle()); // Returns scene handle - much simpler now!

    void addSceneInstance(Asc::Handle fromHandle, Asc::Handle toHandle, Asc::Handle parentHandle = Asc::Handle());

    // Descriptor accessors

    // Only need the active 3
    VkDescriptorSetLayout descSLayout_Global() const { return global_->getDescLayout(); }

    // Global UBO update

    void updateGlobal(uint32_t frameIndex) {
        global_->update(*camera_, frameIndex);
    }


    // Filesystem and registry accessors
    Asc::FS& fs() { return *fs_; }
    const Asc::FS& fs() const { return *fs_; }

    Asc::Reg& r() { return fs_->r(); }
    const Asc::Reg& r() const { return fs_->r(); }

    tinyDrawable& drawable() { return *drawable_; }

    const rtSceneRes& sharedRes() const { return sharedRes_; }
    const tinyVk::Device* vkDevice() const { return dvk_; }

    tinyCamera* camera() const { return camera_.get(); }
    tinyGlobal* global() const { return global_.get(); }

    Asc::Handle mainSceneHandle;

    rtScene* scene(Asc::Handle& sceneHandle = Asc::Handle()) {
        sceneHandle = sceneHandle ? sceneHandle : mainSceneHandle;
        return r().get<rtScene>(sceneHandle);
    }

    // File removal with special handling
    enum class DeferRmType {
        Vulkan, Audio
    };

    void fRemove(Asc::Handle fileHandle);
    void execDeferredRms(DeferRmType type);
    bool hasDeferredRms(DeferRmType type) const;

private:
    const tinyVk::Device* dvk_;

    // File removals that need special handling
    UnorderedMap<DeferRmType, std::vector<Asc::Handle>> deferredRms_;

    UniquePtr<tinyGlobal> global_;
    UniquePtr<tinyCamera> camera_;

    UniquePtr<Asc::FS> fs_;
    UniquePtr<tinyDrawable> drawable_;
    rtSceneRes sharedRes_;

    // void setupFS();
    // void vkCreateResources();
    // void vkCreateDefault();
    void setupResources();

    template<typename T>
    Asc::Handle rAdd(T&& resource) {
        return r().emplace<T>(std::forward<T>(resource));
    }
};