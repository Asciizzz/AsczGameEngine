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

    // Require move semantics to invalidate to avoid instant decrement
    tinyMaterialVk(tinyMaterialVk&& other) noexcept
    : name(std::move(other.name)),
        albTex_(other.albTex_),
        nrmlTex_(other.nrmlTex_),
        deviceVk_(other.deviceVk_),
        defTex_(other.defTex_)
    {
        // null out the old object so its destructor doesn't decrement
        other.albTex_ = nullptr;
        other.nrmlTex_ = nullptr;
    }
    tinyMaterialVk& operator=(tinyMaterialVk&& other) noexcept {
        if (this != &other) {
            // Decrement current textures
            if (albTex_) albTex_->decrementUse();
            if (nrmlTex_) nrmlTex_->decrementUse();

            name = std::move(other.name);
            albTex_ = other.albTex_;
            nrmlTex_ = other.nrmlTex_;
            deviceVk_ = other.deviceVk_;
            defTex_ = other.defTex_;

            other.albTex_ = nullptr;
            other.nrmlTex_ = nullptr;
        }
        return *this;
    }

// -----------------------------------------

    bool setAlbedoTexture(tinyTextureVk* texture) noexcept {
        return setTexture(albTex_, texture);
    }

    bool setNormalTexture(tinyTextureVk* texture) noexcept {
        return setTexture(nrmlTex_, texture); 
    }

private:
    bool setTexture(tinyTextureVk*& curTex, tinyTextureVk*& newTex) noexcept {
        if (!newTex) return false;

        if (curTex) curTex->decrementUse();

        curTex = newTex;
        curTex->incrementUse();
        return true;
    }

    const tinyVk::Device* deviceVk_ = nullptr;
    const tinyTextureVk* defTex_ = nullptr; // Fallback texture

    // Ay guys, I used deque so this won't be dangling, nice
    tinyTextureVk* albTex_ = nullptr; // Albedo texture
    tinyTextureVk* nrmlTex_ = nullptr; // Normal texture
};