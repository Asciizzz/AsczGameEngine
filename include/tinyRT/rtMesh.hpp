#pragma once

#include "tinyRegistry.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "tinyData/tinyMesh.hpp"

namespace tinyRT {

struct MeshRender3D { // Node component
    tinyHandle mesh; // Point to mesh resource in registry
};

}

using rtMeshRender3D = tinyRT::MeshRender3D;
using rtMESHRD3D = tinyRT::MeshRender3D;