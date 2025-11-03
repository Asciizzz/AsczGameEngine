#include "tinyData/tinySharedRes.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

// Descriptor accessors

VkDescriptorPool tinySharedRes::descPool(tinyHandle handle) const {
    if (auto* ptr = fsRegistry->get<tinyVk::DescPool>(handle)) return *ptr;
    return VK_NULL_HANDLE;
}

VkDescriptorSetLayout tinySharedRes::descLayout(tinyHandle handle) const {
    if (auto* ptr = fsRegistry->get<tinyVk::DescSLayout>(handle)) return *ptr;
    return VK_NULL_HANDLE;
}

VkDescriptorSet tinySharedRes::descSet(tinyHandle handle) const {
    if (auto* ptr = fsRegistry->get<tinyVk::DescSet>(handle)) return *ptr;
    return VK_NULL_HANDLE;
}

VkDescriptorPool tinySharedRes::matDescPool() const { return descPool(hMatDescPool); }
VkDescriptorSetLayout tinySharedRes::matDescLayout() const { return descLayout(hMatDescLayout); }

VkDescriptorPool tinySharedRes::skinDescPool() const { return descPool(hSkinDescPool); }
VkDescriptorSetLayout tinySharedRes::skinDescLayout() const { return descLayout(hSkinDescLayout); }

VkDescriptorPool tinySharedRes::mrphDsDescPool() const { return descPool(hMrphDsDescPool); }
VkDescriptorSetLayout tinySharedRes::mrphDsDescLayout() const { return descLayout(hMrphDsDescLayout); }

VkDescriptorPool tinySharedRes::mrphWsDescPool() const { return descPool(hMrphWsDescPool); }
VkDescriptorSetLayout tinySharedRes::mrphWsDescLayout() const { return descLayout(hMrphWsDescLayout); }

// Default resources accessors

#include "tinyData/tinyMaterial.hpp"
#include "tinyData/tinyTexture.hpp"

const tinyMaterialVk* tinySharedRes::defaultMaterialVk() const {
    return fsRegistry->get<tinyMaterialVk>(hDefaultMaterialVk);
}

const tinyTextureVk* tinySharedRes::defaultTextureVk0() const {
    return fsRegistry->get<tinyTextureVk>(hDefaultTextureVk0);
}

const tinyTextureVk* tinySharedRes::defaultTextureVk1() const {
    return fsRegistry->get<tinyTextureVk>(hDefaultTextureVk1);
}

VkDescriptorSet tinySharedRes::dummySkinDescSet() const { return descSet(hDummySkinDescSet); }

VkDescriptorSet tinySharedRes::dummyMeshMrphDsDescSet() const { return descSet(hDummyMeshMrphDsDescSet); }

VkDescriptorSet tinySharedRes::dummyMeshMrphWsDescSet() const { return descSet(hDummyMeshMrphWsDescSet); }
