#pragma once

#include "tinyType.hpp"

namespace tinyRT {

struct MeshRender3D {
    MeshRender3D& assignMesh(tinyHandle meshHandle, const std::vector<float>& morphWeights = {}) noexcept {
        meshHandle_ = meshHandle;
        morphWeights_ = morphWeights;
        return *this;
    }
    MeshRender3D& assignSkeleNode(tinyHandle skeleNodeHandle) noexcept {
        skeleNodeHandle_ = skeleNodeHandle;
        return *this;
    }

    tinyHandle meshHandle() const noexcept { return meshHandle_; }
    tinyHandle skeleNodeHandle() const noexcept { return skeleNodeHandle_; }

    std::vector<float>& morphWeights() noexcept { return morphWeights_; }
    
    const std::vector<float>& morphWeights() const noexcept { return morphWeights_; }

private:
    tinyHandle meshHandle_;
    tinyHandle skeleNodeHandle_;

    std::vector<float> morphWeights_;
};

}

using rtMeshRender3D = tinyRT::MeshRender3D;
using rtMESHRD3D = tinyRT::MeshRender3D;