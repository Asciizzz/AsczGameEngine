#pragma once

#include "tinyType.hpp"

namespace tinyRT {

struct MeshRender3D {
    MeshRender3D& assignMesh(tinyHandle meshHandle, const std::vector<float>& mrphWeights = {}) noexcept {
        meshHandle_ = meshHandle;
        mrphWeights_ = mrphWeights;
        return *this;
    }
    MeshRender3D& assignSkeleNode(tinyHandle skeleNodeHandle) noexcept {
        skeleNodeHandle_ = skeleNodeHandle;
        return *this;
    }

    tinyHandle meshHandle() const noexcept { return meshHandle_; }
    tinyHandle skeleNodeHandle() const noexcept { return skeleNodeHandle_; }

    std::vector<float>& mrphWeights() noexcept { return mrphWeights_; }
    const std::vector<float>& mrphWeights() const noexcept { return mrphWeights_; }

private:
    tinyHandle meshHandle_;
    tinyHandle skeleNodeHandle_;

    std::vector<float> mrphWeights_;
};

}

using rtMeshRender3D = tinyRT::MeshRender3D;
using rtMESHRD3D = tinyRT::MeshRender3D;