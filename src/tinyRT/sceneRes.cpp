#include "sceneRes.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

// Descriptor accessors

VkDescriptorPool sceneRes::descPool(tinyHandle handle) const {
    if (auto* ptr = fsReg->get<tinyVk::DescPool>(handle)) return *ptr;
    return VK_NULL_HANDLE;
}

VkDescriptorSetLayout sceneRes::descLayout(tinyHandle handle) const {
    if (auto* ptr = fsReg->get<tinyVk::DescSLayout>(handle)) return *ptr;
    return VK_NULL_HANDLE;
}

VkDescriptorSet sceneRes::descSet(tinyHandle handle) const {
    if (auto* ptr = fsReg->get<tinyVk::DescSet>(handle)) return *ptr;
    return VK_NULL_HANDLE;
}

VkDescriptorPool sceneRes::matDescPool() const { return descPool(hMatDescPool); }
VkDescriptorSetLayout sceneRes::matDescLayout() const { return descLayout(hMatDescLayout); }

VkDescriptorPool sceneRes::skinDescPool() const { return descPool(hSkinDescPool); }
VkDescriptorSetLayout sceneRes::skinDescLayout() const { return descLayout(hSkinDescLayout); }

VkDescriptorPool sceneRes::mrphDsDescPool() const { return descPool(hMrphDsDescPool); }
VkDescriptorSetLayout sceneRes::mrphDsDescLayout() const { return descLayout(hMrphDsDescLayout); }

VkDescriptorPool sceneRes::mrphWsDescPool() const { return descPool(hMrphWsDescPool); }
VkDescriptorSetLayout sceneRes::mrphWsDescLayout() const { return descLayout(hMrphWsDescLayout); }

// Default resources accessors

#include "tinyMaterial.hpp"
#include "tinyTexture.hpp"

const tinyMaterialVk* sceneRes::defaultMaterialVk() const {
    return fsReg->get<tinyMaterialVk>(hDefaultMaterialVk);
}

const tinyTextureVk* sceneRes::defaultTextureVk0() const {
    return fsReg->get<tinyTextureVk>(hDefaultTextureVk0);
}

const tinyTextureVk* sceneRes::defaultTextureVk1() const {
    return fsReg->get<tinyTextureVk>(hDefaultTextureVk1);
}

VkDescriptorSet sceneRes::dummySkinDescSet() const { return descSet(hDummySkinDescSet); }

VkDescriptorSet sceneRes::dummyMeshMrphDsDescSet() const { return descSet(hDummyMeshMrphDsDescSet); }

VkDescriptorSet sceneRes::dummyMeshMrphWsDescSet() const { return descSet(hDummyMeshMrphWsDescSet); }
