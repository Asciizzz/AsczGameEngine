#pragma once

#include "TinyData/TinyMesh.hpp"
#include "TinyData/TinyMaterial.hpp"
#include "TinyData/TinyTexture.hpp"
#include "TinyData/TinySkeleton.hpp"
#include "TinyData/TinyAnimation.hpp"

struct TinyModel {
    TinyMesh mesh;
    std::vector<int> submeshMaterials; // Material index per submesh
    
    std::vector<TinyMaterial> materials;
    std::vector<TinyTexture> textures;

    TinySkeleton skeleton;
    std::vector<TinyAnimation> animations;
    UnorderedMap<std::string, int> nameToAnimationIndex;

    void printAnimationList() const;
};
