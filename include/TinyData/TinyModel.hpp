#pragma once

#include "TinyData/TinyMesh.hpp"
#include "TinyData/TinyMaterial.hpp"
#include "TinyData/TinyTexture.hpp"
#include "TinyData/TinySkeleton.hpp"
#include "TinyData/TinyAnimation.hpp"
#include "TinyData/TinyNode.hpp"

struct TinyModel {
    // Raw unlinked data
    TinyMesh mesh;
    std::vector<TinyMaterial> materials;
    std::vector<TinyTexture> textures;
    TinySkeleton skeleton;
    std::vector<TinyAnimation> animations;

    // The links
    std::vector<int> meshMaterials; // Only 1 mesh
    UnorderedMap<std::string, int> nameToAnimationIndex;
};

struct TinyModelNew {
    // Raw unlinked registries
    std::vector<TinyMesh> meshes;
    std::vector<std::vector<TinyHandle>> submeshesMats; // Per-mesh material indices

    std::vector<TinyMaterial> materials;
    std::vector<TinyTexture> textures;
    std::vector<TinySkeleton> skeletons;
    std::vector<TinyAnimation> animations;

    std::vector<TinyNode> nodes;
};
