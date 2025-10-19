#pragma once

#include "TinyVK/Resource/TextureVK.hpp"

#include <cstdint>
#include <string>

// Raw texture data (no Vulkan handles)
struct TinyTexture {
    TinyTexture(const TinyTexture&) = delete;
    TinyTexture& operator=(const TinyTexture&) = delete;

    TinyTexture(TinyTexture&&) = default;
    TinyTexture& operator=(TinyTexture&&) = default;

    // Vulkan texture representation
    TinyVK::TextureVK textureVK;

    // Image
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<uint8_t> data;

    std::string name;
    TinyTexture(const std::string& n = "") : name(n) {}

    TinyTexture& setName(const std::string& n);
    TinyTexture& setDimensions(int w, int h);
    TinyTexture& setChannels(int c);
    TinyTexture& setData(const std::vector<uint8_t>& d);

    uint64_t hash = 0; // fnv1a hash of raw data
    uint64_t makeHash();

    // Sampler
    enum class AddressMode {
        Repeat,
        ClampToEdge,
        ClampToBorder
    } addressMode = AddressMode::Repeat;
    TinyTexture& setAddressMode(AddressMode mode) {
        addressMode = mode;
        return *this;
    }


    bool vkCreate(const TinyVK::Device* deviceVK);

    static TinyTexture createDefaultTexture();
};