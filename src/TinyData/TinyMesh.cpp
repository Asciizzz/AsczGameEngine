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



bool TinyMesh::vkCreate(const TinyVK::Device* deviceVK) {
    if (vrtxData.empty() || idxData.empty()) return false;

    vertexBuffer
        .setDataSize(vrtxData.size())
        .setUsageFlags(BufferUsage::Vertex)
        .createDeviceLocalBuffer(deviceVK, vrtxData.data());

    indexBuffer
        .setDataSize(idxData.size())
        .setUsageFlags(BufferUsage::Index)
        .createDeviceLocalBuffer(deviceVK, idxData.data());

    return true;
}

VkIndexType TinyMesh::sizeToIndexType(size_t size) {
    switch (size) {
        case sizeof(uint8_t):  return VK_INDEX_TYPE_UINT8;
        case sizeof(uint16_t): return VK_INDEX_TYPE_UINT16;
        case sizeof(uint32_t): return VK_INDEX_TYPE_UINT32;
        default: throw std::runtime_error("Unsupported index size in TinyMesh");
    }
}