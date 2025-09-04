#pragma once

#include <Az3D/TinyModel.hpp>

namespace Az3D {

struct TempModel {
    TinySubmesh mesh;
    TinySkeleton skeleton;
};


class TinyLoader {
public:
    struct LoadOptions {
        bool loadMaterials = true;
        bool loadTextures = true;
        bool loadSkeleton = true;
    };


    static TinyTexture loadImage(const std::string& filePath);

    // StaticMesh loading functions
    static TinySubmesh loadStaticMesh(const std::string& filePath);
    static TinySubmesh loadStaticMeshFromOBJ(const std::string& filePath);
    static TinySubmesh loadStaticMeshFromGLTF(const std::string& filePath);

    // Temporary model for soon-to-be-legacy resource structure
    static TempModel loadTempModel(const std::string& filePath, bool loadRig = false);

    // True implementation that we will use in the future
    static TinyModel loadModel(const std::string& filePath, const LoadOptions& options = LoadOptions());
};

} // namespace Az3D
