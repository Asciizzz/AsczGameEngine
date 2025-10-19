#include "TinyData/TinyMesh.hpp"

#include <stdexcept>

using namespace TinyVK;

TinyMesh& TinyMesh::setSubmeshes(const std::vector<TinySubmesh>& subs) {
    submeshes = subs;
    return *this;
}

TinyMesh& TinyMesh::addSubmesh(const TinySubmesh& sub) {
    submeshes.push_back(sub);
    return *this;
}

TinyMesh& TinyMesh::writeSubmesh(const TinySubmesh& sub, uint32_t index) {
    if (index < submeshes.size()) submeshes[index] = sub;
    return *this;
}

TinyMesh::IndexType TinyMesh::sizeToIndexType(size_t size) {
    switch (size) {
        case sizeof(uint8_t):  return IndexType::Uint8;
        case sizeof(uint16_t): return IndexType::Uint16;
        case sizeof(uint32_t): return IndexType::Uint32;
        default:               return IndexType::Uint32;
    }
}




bool TinyMesh::createBuffers(const TinyVK::Device* deviceVK) {
    if (vertexData.empty() || indexData.empty()) return false;

    vertexBuffer
        .setDataSize(vertexData.size())
        .setUsageFlags(BufferUsage::Vertex)
        .createDeviceLocalBuffer(deviceVK, vertexData.data());

    indexBuffer
        .setDataSize(indexData.size())
        .setUsageFlags(BufferUsage::Index)
        .createDeviceLocalBuffer(deviceVK, indexData.data());

    return true;
}

VkIndexType TinyMesh::tinyToVkIndexType(TinyMesh::IndexType type) {
    switch (type) {
        case TinyMesh::IndexType::Uint8:  return VK_INDEX_TYPE_UINT8;
        case TinyMesh::IndexType::Uint16: return VK_INDEX_TYPE_UINT16;
        case TinyMesh::IndexType::Uint32: return VK_INDEX_TYPE_UINT32;
        default: throw std::runtime_error("Unsupported index type in TinyMesh");
    }
}