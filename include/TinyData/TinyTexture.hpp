#pragma once

#include "tinyVk/Resource/TextureVK.hpp"

#include <cstdint>
#include <string>

// Raw texture data (no Vulkan handles)
struct tinyTexture {
    tinyTexture(const tinyTexture&) = delete;
    tinyTexture& operator=(const tinyTexture&) = delete;

    tinyTexture(tinyTexture&&) = default;
    tinyTexture& operator=(tinyTexture&&) = default;

    // Vulkan texture representation
    tinyVk::TextureVK textureVK;

    // Image
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<uint8_t> data;

    std::string name;
    tinyTexture(const std::string& n = "") : name(n) {}

    tinyTexture& setName(const std::string& n);
    tinyTexture& setDimensions(int w, int h);
    tinyTexture& setChannels(int c);
    tinyTexture& setData(const std::vector<uint8_t>& d);

    uint64_t hash = 0; // fnv1a hash of raw data
    uint64_t makeHash();

    // Sampler
    enum class AddressMode {
        Repeat,
        ClampToEdge,
        ClampToBorder
    } addressMode = AddressMode::Repeat;
    tinyTexture& setAddressMode(AddressMode mode) {
        addressMode = mode;
        return *this;
    }


    bool vkCreate(const tinyVk::Device* deviceVk);

    static tinyTexture createDefaultTexture();
};