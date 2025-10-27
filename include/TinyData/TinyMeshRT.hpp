#pragma once

#include "TinyData/TinyMesh.hpp"
#include "TinyExt/TinyRegistry.hpp"

struct TinyMeshRT {
    const TinyRegistry* fsRegistry = nullptr;

    TinyHandle meshHandle;
};