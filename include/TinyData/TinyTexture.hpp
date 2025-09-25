#pragma once

#include <vector>
#include <cstdint>

// Raw texture data (no Vulkan handles)
struct TinyTexture {
    // Image
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<uint8_t> data;

    uint64_t hash = 0; // fnv1a hash of raw data
    uint64_t makeHash();

    // Sampler
    enum class AddressMode {
        Repeat,
        ClampToEdge,
        ClampToBorder
    } addressMode = AddressMode::Repeat;

    static TinyTexture createDefaultTexture();
};