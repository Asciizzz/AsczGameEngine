#pragma once

#include "tinyExt/tinyRegistry.hpp"
#include "vulkan/vulkan.h"

// Forward declarations
namespace tinyVk {
    class Device;
}

struct tinyMaterialVk;
struct tinyTextureVk;

struct tinySharedRes {
    uint32_t maxFramesInFlight = 0; // If you messed this up the app just straight up jump off a cliff

    tinyRegistry* fsRegistry = nullptr; // For stuffs and things
    const tinyVk::Device* deviceVk = nullptr;   // For GPU resource creation

// File system helper

    template<typename T> tinyPool<T>& fsView() { return fsRegistry->view<T>(); }
    template<typename T> const tinyPool<T>& fsView() const { return fsRegistry->view<T>(); }

    template<typename T> T* fsGet(tinyHandle handle) { return fsRegistry->get<T>(handle); }
    template<typename T> const T* fsGet(tinyHandle handle) const { return fsRegistry->get<T>(handle); }

// Static vulkan resources

    // Helpful generic accessors
    VkDescriptorPool descPool(tinyHandle handle) const;
    VkDescriptorSetLayout descLayout(tinyHandle handle) const;
    VkDescriptorSet descSet(tinyHandle handle) const;

    // Material (props + textures)
    tinyHandle hMatDescPool;
    tinyHandle hMatDescLayout;
    VkDescriptorPool matDescPool() const;
    VkDescriptorSetLayout matDescLayout() const;

    // Skin (glm::mat4 bones)
    tinyHandle hSkinDescPool;
    tinyHandle hSkinDescLayout;
    VkDescriptorPool skinDescPool() const;
    VkDescriptorSetLayout skinDescLayout() const;

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

    // Default implies that something is usable by "default"
    // Dummy just straight up represents a lack of something lol

    // Material and texture
    tinyHandle hDefaultMaterialVk;
    const tinyMaterialVk* defaultMaterialVk() const;

    tinyHandle hDefaultTextureVk0;
    tinyHandle hDefaultTextureVk1;
    const tinyTextureVk* defaultTextureVk0() const;
    const tinyTextureVk* defaultTextureVk1() const;

    tinyHandle hDummySkinDescSet;
    VkDescriptorSet dummySkinDescSet() const;

    tinyHandle hDummyMeshMrphDsDescSet;
    tinyHandle hDummyMeshMrphWsDescSet;
    VkDescriptorSet dummyMeshMrphDsDescSet() const;
    VkDescriptorSet dummyMeshMrphWsDescSet() const;
};
