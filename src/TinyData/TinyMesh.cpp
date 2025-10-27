#include "TinyData/TinyMesh.hpp"

#include <stdexcept>

using namespace TinyVK;

TinyMesh& TinyMesh::setSubmeshes(const std::vector<TinySubmesh>& subs) {
    submeshes_ = subs;
    return *this;
}

TinyMesh& TinyMesh::addSubmesh(const TinySubmesh& sub) {
    submeshes_.push_back(sub);
    return *this;
}

TinyMesh& TinyMesh::writeSubmesh(const TinySubmesh& sub, uint32_t index) {
    if (index < submeshes_.size()) submeshes_[index] = sub;
    return *this;
}



bool TinyMesh::vkCreate(const TinyVK::Device* deviceVK) {
    if (vrtxData_.empty() || idxData_.empty()) return false;

    vrtxBuffer_
        .setDataSize(vrtxData_.size())
        .setUsageFlags(BufferUsage::Vertex)
        .createDeviceLocalBuffer(deviceVK, vrtxData_.data());

    idxBuffer_ 
        .setDataSize(idxData_.size())
        .setUsageFlags(BufferUsage::Index)
        .createDeviceLocalBuffer(deviceVK, idxData_.data());

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