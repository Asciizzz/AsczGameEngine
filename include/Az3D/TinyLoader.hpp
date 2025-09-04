#pragma once

#include <Az3D/TinyModel.hpp>

namespace Az3D {

struct TempModel {
    TinyMesh mesh;
    TinySkeleton skeleton;
};


class TinyLoader {
public:
    static TinyTexture loadImage(const std::string& filePath);

    // StaticMesh loading functions
    static TinyMesh loadStaticMesh(const std::string& filePath);
    static TinyMesh loadStaticMeshFromOBJ(const std::string& filePath);
    static TinyMesh loadStaticMeshFromGLTF(const std::string& filePath);

    // RigMesh loading functions
    static TempModel loadRigMesh(const std::string& filePath, bool loadRig=true);
};

} // namespace Az3D
