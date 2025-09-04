#pragma once

#include <Az3D/MeshTypes.hpp>
#include <Az3D/RigSkeleton.hpp>

namespace Az3D {

struct TinyTexture {
    int width = 0;
    int height = 0;
    int channels = 0;
    uint8_t* data = nullptr;

    // Free using stbi_image_free since data is allocated by stbi_load
    void free();
    ~TinyTexture() { free(); }
};

struct TinyModel {
    Mesh mesh;
    RigSkeleton rig;

    struct Index {
        size_t mesh = 0;
        size_t rig = 0;
    } index;
};


class TinyLoader {
public:
    static TinyTexture loadImage(const std::string& filePath);
    
    // StaticMesh loading functions
    static Mesh loadStaticMesh(const std::string& filePath);
    static Mesh loadStaticMeshFromOBJ(const std::string& filePath);
    static Mesh loadStaticMeshFromGLTF(const std::string& filePath);

    // RigMesh loading functions
    static TinyModel loadRigMesh(const std::string& filePath, bool loadRig=true);
};

} // namespace Az3D
