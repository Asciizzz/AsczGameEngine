#include "SceneRes.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

// Descriptor accessors

VkDescriptorPool SceneRes::descPool(tinyHandle handle) const {
    if (auto* ptr = fsReg->get<tinyVk::DescPool>(handle)) return *ptr;
    return VK_NULL_HANDLE;
}

VkDescriptorSetLayout SceneRes::descLayout(tinyHandle handle) const {
    if (auto* ptr = fsReg->get<tinyVk::DescSLayout>(handle)) return *ptr;
    return VK_NULL_HANDLE;
}

VkDescriptorSet SceneRes::descSet(tinyHandle handle) const {
    if (auto* ptr = fsReg->get<tinyVk::DescSet>(handle)) return *ptr;
    return VK_NULL_HANDLE;
}

VkDescriptorPool SceneRes::matDescPool() const { return descPool(hMatDescPool); }
VkDescriptorSetLayout SceneRes::matDescLayout() const { return descLayout(hMatDescLayout); }

VkDescriptorPool SceneRes::skinDescPool() const { return descPool(hSkinDescPool); }
VkDescriptorSetLayout SceneRes::skinDescLayout() const { return descLayout(hSkinDescLayout); }

VkDescriptorPool SceneRes::mrphDsDescPool() const { return descPool(hMrphDsDescPool); }
VkDescriptorSetLayout SceneRes::mrphDsDescLayout() const { return descLayout(hMrphDsDescLayout); }

VkDescriptorPool SceneRes::mrphWsDescPool() const { return descPool(hMrphWsDescPool); }
VkDescriptorSetLayout SceneRes::mrphWsDescLayout() const { return descLayout(hMrphWsDescLayout); }

// Default resources accessors

#include "tinyMaterial.hpp"
#include "tinyTexture.hpp"

const tinyMaterialVk* SceneRes::defaultMaterialVk() const {
    return fsReg->get<tinyMaterialVk>(hDefaultMaterialVk);
}

const tinyTextureVk* SceneRes::defaultTextureVk0() const {
    return fsReg->get<tinyTextureVk>(hDefaultTextureVk0);
}

const tinyTextureVk* SceneRes::defaultTextureVk1() const {
    return fsReg->get<tinyTextureVk>(hDefaultTextureVk1);
}

VkDescriptorSet SceneRes::dummySkinDescSet() const { return descSet(hDummySkinDescSet); }

VkDescriptorSet SceneRes::dummyMeshMrphDsDescSet() const { return descSet(hDummyMeshMrphDsDescSet); }

VkDescriptorSet SceneRes::dummyMeshMrphWsDescSet() const { return descSet(hDummyMeshMrphWsDescSet); }
