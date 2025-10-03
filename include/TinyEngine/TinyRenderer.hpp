#pragma once

#include "TinyVK/Render/Swapchain.hpp"
#include "TinyVK/Render/RenderPass.hpp"
#include "TinyVK/Render/DepthManager.hpp"

class TinyRenderer {
public:
    TinyRenderer() = default;
    ~TinyRenderer() = default;

    // Delete copy semantics
    TinyRenderer(const TinyRenderer&) = delete;
    TinyRenderer& operator=(const TinyRenderer&) = delete;

};