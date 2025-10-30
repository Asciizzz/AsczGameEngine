#pragma once

#include "tinyData/tinyMesh.hpp"
#include "tinyData/tinyMaterial.hpp"
#include "tinyData/tinyTexture.hpp"
#include "tinyData/tinySkeleton.hpp"
#include "tinyData/tinyRT_Anime3D.hpp"

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
        glm::mat4 TRFM3D = glm::mat4(1.0f);

        // Mesh render3D
        int MESHR_meshIndx = -1;
        int MESHR_skeleNodeIndx = -1;

        // Bone attach3D
        int BONE3D_skeleNodeIndx = -1;
        int BONE3D_boneIndx = -1;

        // Skeleton3D
        int SKEL3D_skeleIndx = -1;

        // Anime3D
        int ANIM3D_animeIndx = -1;

        bool hasMESHR() const { return MESHR_meshIndx >= 0; }
        bool hasBONE3D() const { return BONE3D_skeleNodeIndx >= 0 && BONE3D_boneIndx >= 0; }
        bool hasSKEL3D() const { return SKEL3D_skeleIndx >= 0; }
        bool hasANIM3D() const { return ANIM3D_animeIndx >= 0; }
    };

    struct Material {
        std::string name;

        int albIndx = -1;
        int nrmlIndx = -1;
    };

    std::string name;

    // All local space
    std::vector<tinyMesh> meshes;
    std::vector<Material> materials;
    std::vector<tinyTexture> textures;
    std::vector<tinySkeleton> skeletons;
    std::vector<tinyRT_ANIM3D> animations;

    std::vector<Node> nodes;
};
