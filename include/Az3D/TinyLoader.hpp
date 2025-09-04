#pragma once

#include <Az3D/Model.hpp>

namespace Az3D {

struct TinyModel {
    Mesh mesh;
    Skeleton rig;

    struct Index {
        size_t mesh = 0;
        size_t rig = 0;
    } index;
};


class TinyLoader {
public:
    static Texture loadImage(const std::string& filePath);
    
    // StaticMesh loading functions
    static Mesh loadStaticMesh(const std::string& filePath);
    static Mesh loadStaticMeshFromOBJ(const std::string& filePath);
    static Mesh loadStaticMeshFromGLTF(const std::string& filePath);

    // RigMesh loading functions
    static TinyModel loadRigMesh(const std::string& filePath, bool loadRig=true);
};

} // namespace Az3D
