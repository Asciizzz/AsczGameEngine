#pragma once

#include "tinyExt/tinyPool.hpp"
#include "tinyData/tinyTexture.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

struct tinyMaterialVk {
    tinyMaterialVk() noexcept = default;
    
    tinyMaterialVk(const tinyMaterialVk&) = delete;
    tinyMaterialVk& operator=(const tinyMaterialVk&) = delete;

    tinyMaterialVk(tinyMaterialVk&&) noexcept = default;
    tinyMaterialVk& operator=(tinyMaterialVk&&) noexcept = default;

// -----------------------------------------

    std::string name; // Material name from glTF

private:
    const tinyVk::Device* deviceVk_ = nullptr;
    const tinyPool<tinyTextureVk>* texturePool_ = nullptr; // to validate texture during runtime
    const tinyTextureVk* defaultTexture_ = nullptr; // Fallback texture

    tinyHandle albHandle_; // Albedo texture
    tinyHandle nrmlHandle_; // Normal texture
};