#include "tinyData/tinyMesh.hpp"

#include <stdexcept>

using namespace tinyVK;

tinyMesh& tinyMesh::setSubmeshes(const std::vector<tinySubmesh>& subs) {
    submeshes_ = subs;
    return *this;
}

tinyMesh& tinyMesh::addSubmesh(const tinySubmesh& sub) {
    submeshes_.push_back(sub);
    return *this;
}

tinyMesh& tinyMesh::writeSubmesh(const tinySubmesh& sub, uint32_t index) {
    if (index < submeshes_.size()) submeshes_[index] = sub;
    return *this;
}



bool tinyMesh::vkCreate(const tinyVK::Device* deviceVK) {
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

VkIndexType tinyMesh::sizeToIndexType(size_t size) {
    switch (size) {
        case sizeof(uint8_t):  return VK_INDEX_TYPE_UINT8;
        case sizeof(uint16_t): return VK_INDEX_TYPE_UINT16;
        case sizeof(uint32_t): return VK_INDEX_TYPE_UINT32;
        default: throw std::runtime_error("Unsupported index size in tinyMesh");
    }
}