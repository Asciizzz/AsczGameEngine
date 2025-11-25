#pragma once

#include "tinyType.hpp"

namespace tinyRT {

struct MeshRender3D {
    tinyHandle mesh;
    tinyHandle material;
    tinyHandle shader;
};

}

using rtMeshRender3D = tinyRT::MeshRender3D;
using rtMESHRD3D = tinyRT::MeshRender3D;