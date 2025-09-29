#include "TinyEngine/TinyRegistry.hpp"

#include <stdexcept>

using namespace AzVulk;

void TinyRegistry::setMaxMaterialCount(uint32_t count) {
    maxMaterialCount = count;
    createMaterialVkResources();
}

void TinyRegistry::setMaxTextureCount(uint32_t count) {
    maxTextureCount = count;
    createTextureVkResources();
}




VkIndexType TinyRegistry::MeshData::tinyToVkIndexType(TinyMesh::IndexType type) {
    switch (type) {
        case TinyMesh::IndexType::Uint8:  return VK_INDEX_TYPE_UINT8;
        case TinyMesh::IndexType::Uint16: return VK_INDEX_TYPE_UINT16;
        case TinyMesh::IndexType::Uint32: return VK_INDEX_TYPE_UINT32;
        default: throw std::runtime_error("Unsupported index type in TinyMesh");
    }
}


// Vulkan resources creation

void TinyRegistry::createMaterialVkResources() {
    if (maxMaterialCount == 0) throw std::runtime_error("TinyRegistry: maxMaterialCount cannot be zero");

    // Create descriptor layout for materials
    matDescLayout = MakeUnique<DescLayout>();
    matDescLayout->create(deviceVK->lDevice, {
        {0, DescType::StorageBuffer, maxMaterialCount, ShaderStage::Fragment, nullptr}
    });

    // Create descriptor pool
    matDescPool = MakeUnique<DescPool>();
    matDescPool->create(deviceVK->lDevice, {
        {DescType::StorageBuffer, maxMaterialCount}
    }, 1);

    // Resize
    materialDatas.resize(maxMaterialCount);

    // Create material buffer
    matBuffer = MakeUnique<DataBuffer>();
    matBuffer->setDataSize(sizeof(MaterialData) * maxMaterialCount)
             .setUsageFlags(BufferUsage::Storage | BufferUsage::TransferDst)
             .setMemPropFlags(MemProp::HostVisibleAndCoherent)
             .createBuffer(deviceVK)
             .mapAndCopy(materialDatas.data());

    // Allocate descriptor set
    matDescSet = MakeUnique<DescSet>();
    matDescSet->allocate(deviceVK->lDevice, *matDescPool, *matDescLayout);

    // Write storage buffer info
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = *matBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = VK_WHOLE_SIZE;

    DescWrite()
        .setDstSet(*matDescSet)
        .setDstBinding(0)
        .setDescType(DescType::StorageBuffer)
        .setDescCount(1)
        .setBufferInfo({ bufferInfo })
        .updateDescSet(deviceVK->lDevice);
}

void TinyRegistry::createTextureVkResources() {
    if (maxTextureCount == 0) throw std::runtime_error("TinyRegistry: maxTextureCount cannot be zero");

    // Create descriptor layout for textures
    texDescLayout = MakeUnique<DescLayout>();
    texDescLayout->create(deviceVK->lDevice, {
        {0, DescType::CombinedImageSampler, maxTextureCount, ShaderStage::Fragment, nullptr}
    });

    // Create descriptor pool
    texDescPool = MakeUnique<DescPool>();
    texDescPool->create(deviceVK->lDevice, {
        {DescType::CombinedImageSampler, maxTextureCount}
    }, 1);

    // Resize
    textureDatas.resize(maxTextureCount);

    // Allocate descriptor set
    texDescSet = MakeUnique<DescSet>();
    texDescSet->allocate(deviceVK->lDevice, *texDescPool, *texDescLayout);

    // Note: Actual image infos will be written when textures are added to the registry
}