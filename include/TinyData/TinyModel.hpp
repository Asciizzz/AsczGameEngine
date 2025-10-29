#pragma once

#include "tinyData/tinyMesh.hpp"
#include "tinyData/tinyMaterial.hpp"
#include "tinyData/tinyTexture.hpp"
#include "tinyData/tinySkeleton.hpp"
#include "tinyData/tinyRT_Anime3D.hpp"
#include "tinyData/tinyRT_Node.hpp"

// lightweight structure to hold primitive model data

struct tinyModel {
    struct Node {
        std::string name;
        Node(const std::string& nodeName = "") : name(nodeName) {}

        int parent = -1;
        std::vector<int> children;

        void addChild(int index) {
            children.push_back(index);
        }
        void setParent(int index) {
            parent = index;
        }

        // Lightweight component data

        // Transform3D
        glm::mat4 T3D = glm::mat4(1.0f);

        // Mesh render3D
        int MR3D_meshIndx = -1;
        int MR3D_skeleNodeIndx = -1;

        // Bone attach3D
        int BA3D_skeleNodeIndx = -1;
        int BA3D_boneIndx = -1;

        // Skeleton3D
        int SK3D_skeleIndx = -1;

        // Anime3D
        int AN3D_animeIndx = -1;

        bool hasMR3D() const { return MR3D_meshIndx >= 0; }
        bool hasBA3D() const { return BA3D_skeleNodeIndx >= 0 && BA3D_boneIndx >= 0; }
        bool hasSK3D() const { return SK3D_skeleIndx >= 0; }
        bool hasAN3D() const { return AN3D_animeIndx >= 0; }
    };

    std::string name;

    // All local space
    std::vector<tinyMesh> meshes;
    std::vector<tinyMaterial> materials;
    std::vector<tinyTexture> textures;
    std::vector<tinySkeleton> skeletons;
    std::vector<tinyRT_AN3D> animations;

    std::vector<tinyModel::Node> nodes;
};
