#include "TinyEngine/TinyResource.hpp"

using namespace AzVulk;

void TinyResource::setMaxMaterialCount(uint32_t count) {
    maxMaterialCount = count;
    createMaterialDescResources(count);
}

void TinyResource::setMaxTextureCount(uint32_t count) {
    maxTextureCount = count;
    createTextureDescResources(count);
}


void TinyResource::createMaterialDescResources(uint32_t count) {
    if (count == 0) return;

    matDescPool = MakeUnique<DescPool>(deviceVK->lDevice);
    matDescPool->create({ { DescType::StorageBuffer, 1 } }, count);

    matDescLayout = MakeUnique<DescLayout>(deviceVK->lDevice);
    matDescLayout->create({ {0, DescType::StorageBuffer, 1, ShaderStage::VertexAndFragment, nullptr} } );
}

void TinyResource::createTextureDescResources(uint32_t count) {
    if (count == 0) return;

    texDescPool = MakeUnique<DescPool>(deviceVK->lDevice);
    texDescPool->create({ { DescType::CombinedImageSampler, 1 } }, count);

    texDescLayout = MakeUnique<DescLayout>(deviceVK->lDevice);
    texDescLayout->create({ {0, DescType::CombinedImageSampler, 1, ShaderStage::Fragment, nullptr} } );
}