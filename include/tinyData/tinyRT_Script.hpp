#pragma once

#include "tinyExt/tinyHandle.hpp"


namespace tinyRT {

struct Scene;
struct Node;

struct Script {
    Script() noexcept = default;

    void haveFun(Scene* scene, tinyHandle nodeHandle, float dTime);
};

} // namespace tinyRT

using tinyRT_SCRIPT = tinyRT::Script;
