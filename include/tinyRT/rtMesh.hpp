#pragma once

#include "tinyType.hpp"

namespace tinyRT {

struct MeshRender3D {
    tinyHandle mesh;
    tinyHandle skeleNode; // NOT skeleton, it's skeleton node
};

}

using rtMeshRender3D = tinyRT::MeshRender3D;
using rtMESHRD3D = tinyRT::MeshRender3D;