#include "Tiny3D/TinyMesh.hpp"

TinySubmesh& TinySubmesh::setMaterial(int index) {
    matIndex = index;
    return *this;
}

TinySubmesh::IndexType TinySubmesh::sizeToIndexType(size_t size) {
    switch (size) {
        case sizeof(uint8_t):  return IndexType::Uint8;
        case sizeof(uint16_t): return IndexType::Uint16;
        case sizeof(uint32_t): return IndexType::Uint32;
        default: return IndexType::Uint32; // Default to uint32
    }
}