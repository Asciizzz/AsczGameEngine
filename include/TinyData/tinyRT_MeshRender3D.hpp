#pragma once

#include "tinyData/tinyMesh.hpp"
#include "tinyExt/tinyRegistry.hpp"
#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

namespace tinyRT {

struct MeshRender3D {
    MeshRender3D() = default;

    MeshRender3D(const MeshRender3D&) = delete;
    MeshRender3D& operator=(const MeshRender3D&) = delete;

    MeshRender3D(MeshRender3D&&) = default;
    MeshRender3D& operator=(MeshRender3D&&) = default;

private:
    tinyHandle meshHandle_;
    const tinyRegistry* fsRegistry_ = nullptr;
    const tinyVk::Device* deviceVk_ = nullptr;
};

} // namespace tinyRT

using tinyRT_MR3D = tinyRT::MeshRender3D;