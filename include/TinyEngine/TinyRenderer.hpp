#pragma once

#include "tinyVK/Render/Swapchain.hpp"
#include "tinyVK/Render/RenderPass.hpp"
#include "tinyVK/Render/DepthImage.hpp"

class tinyRenderer {
public:
    tinyRenderer() = default;
    ~tinyRenderer() = default;

    // Delete copy semantics
    tinyRenderer(const tinyRenderer&) = delete;
    tinyRenderer& operator=(const tinyRenderer&) = delete;

};