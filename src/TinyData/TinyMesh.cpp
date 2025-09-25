#include "TinyData/TinyMesh.hpp"

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