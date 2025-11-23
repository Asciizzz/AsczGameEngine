#pragma once

#include "tinyFS.hpp"

// #include "tinyRT_MeshRender3D.hpp"
// #include "tinyRT_Skeleton3D.hpp"
// #include "tinyRT_Anime3D.hpp"
// #include "tinyRT_Script.hpp"
// #include "tinyRT_Node.hpp"

#include "sceneRes.hpp"
#include "rtNode.hpp"

// Oh boy we starting fresh, this is gonna be fun
namespace tinyRT {

class Scene {
    tinyPool<rtNode> nodes_;
    sceneRes res_;

public:

    tinyRegistry rt_;

    sceneRes& res() noexcept { return res_; }
};

} // namespace tinyRT

using rtScene = tinyRT::Scene;