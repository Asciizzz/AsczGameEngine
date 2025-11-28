#pragma once

#include "tinyMesh.hpp"

namespace tinyRT {

/* Morph Weight explanation:

For example:

Mesh {
    Sub1: 10 targets
    Sub2: 5 targets
    Sub3: no targets
    Sub4: 8 targets
}

-> Flat weight array | 10 | 5 | 0 | 8 | = 23 weights
-> SubMorph info:
    Sub1: offset 0,  count 10
    Sub2: offset 10, count 5
    Sub3: offset 15, count 0
    Sub4: offset 15, count 8

When assigning weights to MeshRender3D, user need to provide the full flat array of weights.

*/

struct MeshRender3D {

    MeshRender3D& assignMesh(tinyHandle meshHandle, const tinyMesh* mesh = nullptr) noexcept {
        if (!mesh) return *this;

        meshHandle_ = meshHandle;

        // For each submesh, prepare morph target info
        subMrphs_.clear();

        uint32_t totalTargets = 0;
        for (size_t subIdx = 0; subIdx < mesh->submeshes().size(); ++subIdx) {
            const auto& submesh = mesh->submeshes()[subIdx];
            uint32_t targetCount = static_cast<uint32_t>(submesh.mrphTargets.size());

            SubMorph sm;
            sm.offset = totalTargets;
            sm.count  = targetCount;

            subMrphs_.push_back(sm);

            totalTargets += targetCount;
        }

        mrphWs_.resize(totalTargets, 0.0f);

        return *this;
    }

    MeshRender3D& copy(const MeshRender3D* other) noexcept {
        meshHandle_ = other->meshHandle_;
        skeleNodeHandle_ = other->skeleNodeHandle_;

        mrphWs_ = other->mrphWs_;
        subMrphs_ = other->subMrphs_;
        return *this;
    }

    MeshRender3D& assignSkeleNode(tinyHandle skeleNodeHandle) noexcept {
        skeleNodeHandle_ = skeleNodeHandle;
        return *this;
    }

    tinyHandle meshHandle() const noexcept { return meshHandle_; }
    tinyHandle skeleNodeHandle() const noexcept { return skeleNodeHandle_; }

    struct SubMorph {
        uint32_t offset = 0;
        uint32_t count  = 0;
    };

    std::vector<float>& mrphWeights() noexcept { return mrphWs_; }
    const std::vector<float>& mrphWeights() const noexcept { return mrphWs_; }

    const std::vector<SubMorph>& subMrphs() const noexcept { return subMrphs_; }

    const SubMorph* subMrph(uint32_t index) const noexcept {
        return index < subMrphs_.size() ? &subMrphs_[index] : nullptr;
    }

private:
    tinyHandle meshHandle_;
    tinyHandle skeleNodeHandle_;

    std::vector<float> mrphWs_; // Flat weights for morph targets
    std::vector<SubMorph> subMrphs_; // Submesh morph target info
};

}

using rtMeshRender3D = tinyRT::MeshRender3D;
using rtMESHRD3D = tinyRT::MeshRender3D;