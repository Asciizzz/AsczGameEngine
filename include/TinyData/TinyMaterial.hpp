#pragma once

#include "tinyExt/tinyPool.hpp"
#include "tinyData/tinyTexture.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

struct tinyMaterialVk {
    std::string name;
    tinyMaterialVk() noexcept = default;

    ~tinyMaterialVk() {
        // Decrease texture use counts
        if (albTex_) albTex_->decrementUse();
        if (nrmlTex_) nrmlTex_->decrementUse();
    }

    tinyMaterialVk(const tinyMaterialVk&) = delete;
    tinyMaterialVk& operator=(const tinyMaterialVk&) = delete;

    tinyMaterialVk(tinyMaterialVk&&) noexcept = default;
    tinyMaterialVk& operator=(tinyMaterialVk&&) noexcept = default;

// -----------------------------------------

    bool setAlbedoTexture(tinyTextureVk* texture) {
        if (!texture) return false;

        // if another texture exist, decrease its use count
        if (albTex_) albTex_->decrementUse();

        albTex_ = texture;
        albTex_->incrementUse();
    }

private:
    const tinyVk::Device* deviceVk_ = nullptr;
    const tinyTextureVk* defTex_ = nullptr; // Fallback texture

    // need to be modifiable for usage write
    tinyTextureVk* albTex_ = nullptr; // Albedo texture
    tinyTextureVk* nrmlTex_ = nullptr; // Normal texture
};