#include "SceneRes.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

#include "tinyCamera.hpp"

// Descriptor accessors

VkDescriptorPool SceneRes::descPool(tinyHandle handle) const {
    if (auto* ptr = fsr->get<tinyVk::DescPool>(handle)) return *ptr;
    return VK_NULL_HANDLE;
}

VkDescriptorSetLayout SceneRes::descLayout(tinyHandle handle) const {
    if (auto* ptr = fsr->get<tinyVk::DescSLayout>(handle)) return *ptr;
    return VK_NULL_HANDLE;
}

VkDescriptorSet SceneRes::descSet(tinyHandle handle) const {
    if (auto* ptr = fsr->get<tinyVk::DescSet>(handle)) return *ptr;
    return VK_NULL_HANDLE;
}

VkDescriptorPool SceneRes::mrphDsDescPool() const { return descPool(hMrphDsDescPool); }
VkDescriptorSetLayout SceneRes::mrphDsDescLayout() const { return descLayout(hMrphDsDescLayout); }

VkDescriptorPool SceneRes::mrphWsDescPool() const { return descPool(hMrphWsDescPool); }
VkDescriptorSetLayout SceneRes::mrphWsDescLayout() const { return descLayout(hMrphWsDescLayout); }

VkDescriptorSet SceneRes::dummyMeshMrphDsDescSet() const { return descSet(hDummyMeshMrphDsDescSet); }
VkDescriptorSet SceneRes::dummyMeshMrphWsDescSet() const { return descSet(hDummyMeshMrphWsDescSet); }
