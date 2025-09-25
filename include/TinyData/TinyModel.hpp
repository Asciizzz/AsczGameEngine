#pragma once

#include "TinyData/TinyMesh.hpp"
#include "TinyData/TinyMaterial.hpp"
#include "TinyData/TinyTexture.hpp"
#include "TinyData/TinySkeleton.hpp"
#include "TinyData/TinyAnimation.hpp"
#include "TinyData/TinyNode.hpp"

struct TinyModel {
    TinyMesh mesh;

    std::vector<TinyMaterial> materials;
    std::vector<TinyTexture> textures;

    TinySkeleton skeleton;
    std::vector<TinyAnimation> animations;
    UnorderedMap<std::string, int> nameToAnimationIndex;
};

struct TinyModelNew {
    // Raw data
    std::vector<TinyMesh> meshes;

    std::vector<TinyMaterial> materials;
    std::vector<TinyTexture> textures;

    std::vector<TinySkeleton> skeletons;
    std::vector<int> skeletonForMesh;

    std::vector<TinyAnimation> animations;
    std::vector<int> animationForSkeleton;

    // Template/Prefab like structure
    std::vector<TinyNode> nodes; // Local scope
};
