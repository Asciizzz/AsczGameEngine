#pragma once

#include "Az3D/TinyMesh.hpp"
#include "Az3D/TinyRig.hpp"

namespace Az3D {

struct TinyModel {
    std::vector<TinySubmesh> submeshes;
    std::vector<TinyMaterial> materials;
    std::vector<TinyTexture> textures;
    TinySkeleton skeleton;

    TinyModel() = default;

    void printDebug() const;

    uint32_t getSubmeshIndexCount(size_t index) const;

    // Future: animations
};

} // namespace Az3D
