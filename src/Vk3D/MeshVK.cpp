#include "Vk3D/MeshVK.hpp"
#include <stdexcept>

using namespace AzVulk;

void SubmeshVK::create(const AzVulk::Device* deviceVK, const TinySubmesh& submesh) {
    const auto& vertexData = submesh.vertexData;
    const auto& indexData = submesh.indexData;

    vertexBuffer
        .cleanup()
        .setDataSize(vertexData.size())
        .setUsageFlags(BufferUsage::Vertex)
        .createDeviceLocalBuffer(deviceVK, vertexData.data());

    indexBuffer
        .cleanup()
        .setDataSize(indexData.size())
        .setUsageFlags(BufferUsage::Index)
        .createDeviceLocalBuffer(deviceVK, indexData.data());

    indexType = tinyToVkIndexType(submesh.indexType);
}

VkIndexType SubmeshVK::tinyToVkIndexType(TinySubmesh::IndexType type) {
    switch (type) {
        case TinySubmesh::IndexType::Uint8:  return VK_INDEX_TYPE_UINT8;
        case TinySubmesh::IndexType::Uint16: return VK_INDEX_TYPE_UINT16;
        case TinySubmesh::IndexType::Uint32: return VK_INDEX_TYPE_UINT32;
        default: return VK_INDEX_TYPE_UINT32;
    }
}