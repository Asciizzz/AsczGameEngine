#pragma once

#include "tinyExt/tinyPool.hpp"
#include "tinyData/tinyTexture.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

struct tinyMaterialVk {
    std::string name;
    tinyMaterialVk() noexcept = default;

    ~tinyMaterialVk() noexcept {
        // Decrease texture use counts
        if (albTex_) albTex_->decrementUse();
        if (nrmlTex_) nrmlTex_->decrementUse();
    }

    tinyMaterialVk(const tinyMaterialVk&) = delete;
    tinyMaterialVk& operator=(const tinyMaterialVk&) = delete;

    tinyMaterialVk(tinyMaterialVk&&) noexcept = default;
    tinyMaterialVk& operator=(tinyMaterialVk&&) noexcept = default;

// -----------------------------------------

    bool setAlbedoTexture(tinyTextureVk* texture) noexcept {
        return setTexture(albTex_, texture);
    }

    bool setNormalTexture(tinyTextureVk* texture) noexcept {
        return setTexture(nrmlTex_, texture); 
    }

private:
    bool setTexture(tinyTextureVk*& oldTex, tinyTextureVk* newTex) noexcept {
        if (!newTex) return false;

        if (oldTex) oldTex->decrementUse();

        oldTex = newTex;
        oldTex->incrementUse();
        return true;
    }

    const tinyVk::Device* deviceVk_ = nullptr;
    const tinyTextureVk* defTex_ = nullptr; // Fallback texture

    // Ay guys, I used deque so this won't be dangling, nice
    tinyTextureVk* albTex_ = nullptr; // Albedo texture
    tinyTextureVk* nrmlTex_ = nullptr; // Normal texture
};