#pragma once

#include "TinyData/TinyMesh.hpp"
#include "TinyData/TinyMaterial.hpp"
#include "TinyData/TinyTexture.hpp"
#include "TinyData/TinySkeleton.hpp"
#include "TinyData/TinyAnimeRT.hpp"
#include "TinyData/TinyNode.hpp"

struct TinyModel {
    std::string name;

    // All local space
    std::vector<TinyMesh> meshes;
    std::vector<TinyMaterial> materials;
    std::vector<TinyTexture> textures;
    std::vector<TinySkeleton> skeletons;
    std::vector<TinyAnimeRT> animations;

    std::vector<TinyNode> nodes;
};
