#pragma once

#include "tinyType.hpp"

namespace tinyRT {

using MorphWeights = std::vector<std::vector<float>>;

struct MeshRender3D {

    MeshRender3D& assignMesh(tinyHandle meshHandle, const MorphWeights& morphs = {}) noexcept {
        meshHandle_ = meshHandle;
        morphs_ = morphs;

        return *this;
    }
    MeshRender3D& assignSkeleNode(tinyHandle skeleNodeHandle) noexcept {
        skeleNodeHandle_ = skeleNodeHandle;
        return *this;
    }

    tinyHandle meshHandle() const noexcept { return meshHandle_; }
    tinyHandle skeleNodeHandle() const noexcept { return skeleNodeHandle_; }

    std::vector<float>* subMrphWeights(uint32_t submeshIndex) noexcept {
        if (submeshIndex >= morphs_.size()) return nullptr;
        return &morphs_[submeshIndex];
    }

    MorphWeights& morphs() noexcept { return morphs_; }
    const MorphWeights& morphs() const noexcept { return morphs_; }

private:
    tinyHandle meshHandle_;
    tinyHandle skeleNodeHandle_;

    // [Submesh's [Weights]]
    MorphWeights morphs_;
};

}

using rtMeshRender3D = tinyRT::MeshRender3D;
using rtMESHRD3D = tinyRT::MeshRender3D;