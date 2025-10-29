#pragma once

#include "tinyData/tinyMesh.hpp"
#include "tinyExt/tinyRegistry.hpp"
#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

namespace tinyRT {

struct MeshRender3D {
    MeshRender3D() = default;
    MeshRender3D* init(const tinyVk::Device* deviceVk, const tinyRegistry* fsRegistry);

    MeshRender3D(const MeshRender3D&) = delete;
    MeshRender3D& operator=(const MeshRender3D&) = delete;

    MeshRender3D(MeshRender3D&&) = default;
    MeshRender3D& operator=(MeshRender3D&&) = default;

// -----------------------------------------

    MeshRender3D& setMesh(tinyHandle meshHandle) {
        meshHandle_ = meshHandle.valid() ? meshHandle : meshHandle_;
        return *this;
    }

    MeshRender3D& setSkeleNode(tinyHandle skeleNodeHandle) {
        skeleNodeHandle_ = skeleNodeHandle.valid() ? skeleNodeHandle : skeleNodeHandle_;
        return *this;
    }

    void copy(const MeshRender3D* other);

// -----------------------------------------

    tinyHandle meshHandle() const { return meshHandle_; }
    tinyHandle skeleNodeHandle() const { return skeleNodeHandle_; }

    const tinyMeshVk* rMesh() const {
        return fsRegistry_ ? fsRegistry_->get<tinyMeshVk>(meshHandle_) : nullptr;
    }

private:
    tinyHandle meshHandle_;
    tinyHandle skeleNodeHandle_; // For skinning

    const tinyRegistry* fsRegistry_ = nullptr;
    const tinyVk::Device* deviceVk_ = nullptr;

    // In the future there will be dedicated buffers and descriptors for morph targets
    // void vkCreate();
};

} // namespace tinyRT

using tinyRT_MESHR = tinyRT::MeshRender3D;