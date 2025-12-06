#pragma once

#include "tinyMesh.hpp"
#include "tinyMaterial.hpp"
#include "tinyTexture.hpp"
#include "tinySkeleton.hpp"
#include "tinyRT_Anime3D.hpp"
#include "tinyScript.hpp"

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

        glm::mat4 TRFM3D = glm::mat4(1.0f);

        // Mesh render3D
        int MESHRD_meshIndx = -1;
        int MESHRD_skeleNodeIndx = -1;

        // Bone attach3D
        int BONE3D_skeleNodeIndx = -1;
        int BONE3D_boneIndx = -1;

        // Skeleton3D
        int SKEL3D_skeleIndx = -1;

        // Anime3D
        int ANIM3D_animeIndx = -1;

        bool hasTRFM3D() const { return TRFM3D != glm::mat4(0.0f); }
        bool hasMESHR() const { return MESHRD_meshIndx >= 0; }
        bool hasBONE3D() const { return BONE3D_skeleNodeIndx >= 0 && BONE3D_boneIndx >= 0; }
        bool hasSKEL3D() const { return SKEL3D_skeleIndx >= 0; }
        bool hasANIM3D() const { return ANIM3D_animeIndx >= 0; }
    };

    struct Material {
        std::string name;

        int albIndx = -1;
        int nrmlIndx = -1;
        int metalIndx = -1;
        int emisIndx = -1;
        glm::vec4 baseColor = glm::vec4(1.0f); // Default white
        
        // Alpha rendering mode:
        // 0 = OPAQUE (default, uses alpha test/discard)
        // 1 = BLEND (transparent, uses alpha blending)
        // 2 = MASK (alpha cutoff at 0.5)
        uint32_t alphaMode = 0;
        
        bool isTransparent() const { return alphaMode == 1; }
        bool isOpaque() const { return alphaMode == 0; }
        bool isAlphaMask() const { return alphaMode == 2; }
    };

    struct Mesh {
        tinyMesh mesh;
        int matIdx = -1;
        std::string name;
    };

    struct Skeleton {
        tinySkeleton skeleton;
        std::string name;
    };

    struct Texture {
        tinyTexture texture;
        std::string name;
    };

    std::string name;

    // All local space
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures;
    std::vector<Skeleton> skeletons;
    std::vector<tinyRT_ANIM3D> animations;

    std::vector<Node> nodes;
};
