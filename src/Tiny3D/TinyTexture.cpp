#include "Tiny3D/TinyTexture.hpp"

#include <vulkan/vulkan.h>

#include <windows.h>
#include <wincrypt.h>
#include <sstream>
#include <iomanip>

uint64_t TinyTexture::makeHash() {
    const uint64_t FNV_offset = 1469598103934665603ULL;
    const uint64_t FNV_prime  = 1099511628211ULL;
    uint64_t h = FNV_offset;

    auto fnv1a = [&](auto value) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        for (size_t i = 0; i < sizeof(value); i++) {
            h ^= bytes[i];
            h *= FNV_prime;
        }
    };

    // Dimensions & channels
    fnv1a(width);
    fnv1a(height);
    fnv1a(channels);

    // Sampler state
    auto addr = static_cast<uint32_t>(addressMode);
    fnv1a(addr);

    // Image data
    for (auto b : data) {
        h ^= b;
        h *= FNV_prime;
    }

    return h;
}

TinyTexture TinyTexture::createDefaultTexture() {
    TinyTexture texture;
    texture.width = 1;
    texture.height = 1;
    texture.channels = 4;
    texture.data = { 255, 255, 255, 255 }; // White pixel
    texture.makeHash();
    return texture;
}