#include "TinyEngine/TinyRegistry.hpp"

#include <stdexcept>

using namespace AzVulk;

void TinyRegistry::setMaxMaterialCount(uint32_t count) {
    maxMaterialCount = count;
    createMaterialDescResources(count);
}

void TinyRegistry::setMaxTextureCount(uint32_t count) {
    maxTextureCount = count;
    createTextureDescResources(count);
}


// void TinyRegistry::createMaterialDescResources(uint32_t count) {
//     if (count == 0) return;

//     matDescPool = MakeUnique<DescPool>(deviceVK->lDevice);
//     matDescPool->create({ { DescType::StorageBuffer, 1 } }, count);

//     matDescLayout = MakeUnique<DescLayout>(deviceVK->lDevice);
//     matDescLayout->create({ {0, DescType::StorageBuffer, 1, ShaderStage::VertexAndFragment, nullptr} } );
// }

// void TinyRegistry::createTextureDescResources(uint32_t count) {
//     if (count == 0) return;

//     texDescPool = MakeUnique<DescPool>(deviceVK->lDevice);
//     texDescPool->create({ { DescType::CombinedImageSampler, 1 } }, count);

//     texDescLayout = MakeUnique<DescLayout>(deviceVK->lDevice);
//     texDescLayout->create({ {0, DescType::CombinedImageSampler, 1, ShaderStage::Fragment, nullptr} } );
// }


void TinyMeshVK::import(const TinyMesh& mesh, const AzVulk::DeviceVK* deviceVK) {
    vertexBuffer
        .setDataSize(mesh.vertexData.size())
        .setUsageFlags(BufferUsage::Vertex)
        .createDeviceLocalBuffer(deviceVK, mesh.vertexData.data());

    indexBuffer
        .setDataSize(mesh.indexData.size())
        .setUsageFlags(BufferUsage::Index)
        .createDeviceLocalBuffer(deviceVK, mesh.indexData.data());

    indexType = tinyToVkIndexType(mesh.indexType);

    submeshes = mesh.submeshes;
}

VkIndexType TinyMeshVK::tinyToVkIndexType(TinyMesh::IndexType type) {
    switch (type) {
        case TinyMesh::IndexType::Uint8:  return VK_INDEX_TYPE_UINT8;
        case TinyMesh::IndexType::Uint16: return VK_INDEX_TYPE_UINT16;
        case TinyMesh::IndexType::Uint32: return VK_INDEX_TYPE_UINT32;
        default: throw std::runtime_error("Unsupported index type in TinyMesh");
    }
}


void TinyMaterialVK::import(const TinyMaterialVK::Data& matData, const AzVulk::DeviceVK* deviceVK,
                        VkDescriptorSetLayout layout, VkDescriptorPool pool) {
}