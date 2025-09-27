#include "TinyEngine/TinyResource.hpp"

#include <stdexcept>

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
    data = matData;

    matBuffer
        .setDataSize(sizeof(Data))
        .setUsageFlags(BufferUsage::Uniform | BufferUsage::TransferDst)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(deviceVK)
        .mapAndCopy(&data);

    matDescSet.init(deviceVK->lDevice);
    matDescSet.allocate(pool, layout, 1);

    // --- bind buffer to descriptor ---
    VkDescriptorBufferInfo materialBufferInfo{};
    materialBufferInfo.buffer = matBuffer;
    materialBufferInfo.offset = 0;
    materialBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = matDescSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = DescType::StorageBuffer;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &materialBufferInfo;

    vkUpdateDescriptorSets(deviceVK->lDevice, 1, &descriptorWrite, 0, nullptr);
}