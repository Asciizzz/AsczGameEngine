#pragma once

#include "tinyExt/tinyRegistry.hpp"
#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

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

    VkDescriptorPool descPool(tinyHandle handle) const {
        if (auto* ptr = fsRegistry->get<tinyVk::DescPool>(handle)) return *ptr;
        return VK_NULL_HANDLE;
    }
    VkDescriptorSetLayout descLayout(tinyHandle handle) const {
        if (auto* ptr = fsRegistry->get<tinyVk::DescSLayout>(handle)) return *ptr;
        return VK_NULL_HANDLE;
    }

    tinyHandle hMatDescPool;
    tinyHandle hMatDescLayout;
    VkDescriptorPool matDescPool() const { return descPool(hMatDescPool); }
    VkDescriptorSetLayout matDescLayout() const { return descLayout(hMatDescLayout); }

    tinyHandle hSkinDescPool;
    tinyHandle hSkinDescLayout;
    VkDescriptorPool skinDescPool() const { return descPool(hSkinDescPool); }
    VkDescriptorSetLayout skinDescLayout() const { return descLayout(hSkinDescLayout); }

    tinyHandle hMrphDsDescPool;
    tinyHandle hMrphDsDescLayout;
    VkDescriptorPool mrphDsDescPool() const { return descPool(hMrphDsDescPool); }
    VkDescriptorSetLayout mrphDsDescLayout() const { return descLayout(hMrphDsDescLayout); }

    tinyHandle hMrphWsDescPool;
    tinyHandle hMrphWsDescLayout;
    VkDescriptorPool mrphWsDescPool() const { return descPool(hMrphWsDescPool); }
    VkDescriptorSetLayout mrphWsDescLayout() const { return descLayout(hMrphWsDescLayout); }
};