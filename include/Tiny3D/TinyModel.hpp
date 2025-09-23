#pragma once

#include "Tiny3D/TinyMesh.hpp"
#include "Tiny3D/TinyMaterial.hpp"
#include "Tiny3D/TinyTexture.hpp"
#include "Tiny3D/TinySkeleton.hpp"
#include "Tiny3D/TinyAnimation.hpp"

struct TinyModel {
    TinyMesh mesh;
    std::vector<TinyMaterial> materials;
    std::vector<TinyTexture> textures;

    TinySkeleton skeleton;
    std::vector<TinyAnimation> animations;
    UnorderedMap<std::string, int> nameToAnimationIndex;

    void printAnimationList() const;
};
