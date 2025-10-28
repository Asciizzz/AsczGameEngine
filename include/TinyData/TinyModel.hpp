#pragma once

#include "tinyData/tinyMesh.hpp"
#include "tinyData/tinyMaterial.hpp"
#include "tinyData/tinyTexture.hpp"
#include "tinyData/tinySkeleton.hpp"
#include "tinyDataRT/tinyAnime3D.hpp"
#include "tinyDataRT/tinyNodeRT.hpp"

struct tinyModel {
    std::string name;

    // All local space
    std::vector<tinyMesh> meshes;
    std::vector<tinyMaterial> materials;
    std::vector<tinyTexture> textures;
    std::vector<tinySkeleton> skeletons;
    std::vector<tinyRT_AN3D> animations;

    std::vector<tinyNodeRT> nodes;
};
