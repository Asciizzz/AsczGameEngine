#include "Tiny3D/TinyMesh.hpp"
#include <windows.h>
#include <wincrypt.h>
#include <sstream>
#include <iomanip>

uint32_t TinyTexture::makeHash() {
    // FNV-1a 32-bit hash
    const uint32_t fnv_prime = 0x01000193;
    const uint32_t fnv_offset_basis = 0x811C9DC5;

    uint32_t hashValue = fnv_offset_basis;
    for (const auto& byte : data) {
        hashValue ^= byte;
        hashValue *= fnv_prime;
    }

    hash = hashValue;
    return hash;
}