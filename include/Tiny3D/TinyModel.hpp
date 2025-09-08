#pragma once

#include "Tiny3D/TinyMesh.hpp"
#include "Tiny3D/TinySkeleton.hpp"
#include "Tiny3D/TinyAnimation.hpp"

struct TinyModel {
    std::vector<TinySubmesh> submeshes;
    std::vector<TinyMaterial> materials;
    std::vector<TinyTexture> textures;

    TinySkeleton skeleton;
    std::vector<TinyAnimation> animations;
    UnorderedMap<std::string, int> nameToAnimationIndex;

    void printDebug() const;
    uint32_t getSubmeshIndexCount(size_t index) const;

    void printAnimationList() const;
};
