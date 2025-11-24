#pragma once


#include "tinyPool.hpp"

#include "tinyData/tinyMaterial.hpp"
#include "tinyData/tinyTexture.hpp"

// A material - texture vulkan management machine

struct MtlMachine {
    void init(const tinyVk::Device* dvk,
            tinyPool<tinyMaterial>* matPool,
            tinyPool<tinyTexture>* texPool) noexcept {
    }


private:
    const tinyVk::Device* dvk_ = nullptr;
    tinyPool<tinyMaterial>* matPool_ = nullptr;
    tinyPool<tinyTexture>* texPool_ = nullptr;
};