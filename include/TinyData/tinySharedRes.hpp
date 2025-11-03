#pragma once

#include "tinyExt/tinyRegistry.hpp"
#include "vulkan/vulkan.h"

// Forward declarations
namespace tinyVk {
    class Device;
}

struct tinySharedRes {
    uint32_t maxFramesInFlight = 0; // If you messed this up the app just straight up jump off a cliff

    const tinyRegistry*   fsRegistry = nullptr; // For stuffs and things
    const tinyVk::Device* deviceVk = nullptr;   // For GPU resource creation

// File system helper

    template<typename T>
    const tinyPool<T>& fsView() const { return fsRegistry->view<T>(); }

    template<typename T>
    const T* fsGet(tinyHandle handle) const { return fsRegistry->get<T>(handle); }

// Descriptor accessors

    VkDescriptorPool descPool(tinyHandle handle) const;
    VkDescriptorSetLayout descLayout(tinyHandle handle) const;

    tinyHandle hMatDescPool;
    tinyHandle hMatDescLayout;
    VkDescriptorPool matDescPool() const;
    VkDescriptorSetLayout matDescLayout() const;

    tinyHandle hSkinDescPool;
    tinyHandle hSkinDescLayout;
    VkDescriptorPool skinDescPool() const;
    VkDescriptorSetLayout skinDescLayout() const;

    tinyHandle hMrphDsDescPool;
    tinyHandle hMrphDsDescLayout;
    VkDescriptorPool mrphDsDescPool() const;
    VkDescriptorSetLayout mrphDsDescLayout() const;

    tinyHandle hMrphWsDescPool;
    tinyHandle hMrphWsDescLayout;
    VkDescriptorPool mrphWsDescPool() const;
    VkDescriptorSetLayout mrphWsDescLayout() const;
};