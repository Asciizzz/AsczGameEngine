#pragma once

#include "tinyVk/Render/Swapchain.hpp"
#include "tinyVk/Render/RenderPass.hpp"
#include "tinyVk/Render/DepthImage.hpp"

class tinyRenderer {
public:
    tinyRenderer() noexcept = default;
    ~tinyRenderer() noexcept = default;

    // Delete copy semantics
    tinyRenderer(const tinyRenderer&) = delete;
    tinyRenderer& operator=(const tinyRenderer&) = delete;

};