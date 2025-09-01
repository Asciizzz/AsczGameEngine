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
    MeshSkinned mesh;
    RigSkeleton rig;
};


class TinyLoader {
public:
    static TinyTexture loadImage(const std::string& filePath);
    
    // MeshStatic loading functions
    static MeshStatic loadMeshStatic(const std::string& filePath);
    static MeshStatic loadMeshStaticFromOBJ(const std::string& filePath);
    static MeshStatic loadMeshStaticFromGLTF(const std::string& filePath);

    // MeshSkinned loading functions
    static TinyModel loadMeshSkinned(const std::string& filePath, bool loadRig=true);
};

} // namespace Az3D
