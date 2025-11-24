#pragma once


#include "tinyRegistry.hpp"
#include "tinyVk/System/Device.hpp"
#include "tinyCamera.hpp"
#include "tinyDrawable.hpp"

struct SceneRes {
    uint32_t maxFramesInFlight = 0; // If you messed this up the app just straight up jump off a cliff

    tinyRegistry* fsr = nullptr; // For stuffs and things
    const tinyVk::Device* dvk = nullptr;   // For GPU resource creation
    tinyCamera* camera = nullptr;    // For global UBOs
    tinyDrawable* drawable = nullptr;

// File system helper

    template<typename T> tinyPool<T>& fsView() { return fsr->view<T>(); }
    template<typename T> const tinyPool<T>& fsView() const { return fsr->view<T>(); }

    template<typename T> T* fsGet(tinyHandle handle) { return fsr->get<T>(handle); }
    template<typename T> const T* fsGet(tinyHandle handle) const { return fsr->get<T>(handle); }

// Static vulkan resources

    // Helpful generic accessors
    VkDescriptorPool descPool(tinyHandle handle) const;
    VkDescriptorSetLayout descLayout(tinyHandle handle) const;
    VkDescriptorSet descSet(tinyHandle handle) const;

    // Morph target deltas (tinyMorph - 3 glm::vec3)
    tinyHandle hMrphDsDescPool;
    tinyHandle hMrphDsDescLayout;
    VkDescriptorPool mrphDsDescPool() const;
    VkDescriptorSetLayout mrphDsDescLayout() const;

    // Morph target weights (float)
    tinyHandle hMrphWsDescPool;
    tinyHandle hMrphWsDescLayout;
    VkDescriptorPool mrphWsDescPool() const;
    VkDescriptorSetLayout mrphWsDescLayout() const;

// Default resources accessors

    tinyHandle hDummyMeshMrphDsDescSet;
    tinyHandle hDummyMeshMrphWsDescSet;
    VkDescriptorSet dummyMeshMrphDsDescSet() const;
    VkDescriptorSet dummyMeshMrphWsDescSet() const;
};
