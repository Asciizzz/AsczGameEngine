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

struct TinyRig {
    SharedPtr<MeshSkinned> mesh;
    SharedPtr<RigSkeleton> skeleton;
};


class TinyLoader {
public:
    static TinyTexture loadImage(const std::string& filePath);
    
    // MeshStatic loading functions
    static SharedPtr<MeshStatic> loadMeshStatic(const std::string& filePath);
    static SharedPtr<MeshStatic> loadMeshStaticFromOBJ(const std::string& filePath);
    static SharedPtr<MeshStatic> loadMeshStaticFromGLTF(const std::string& filePath);

    // MeshSkinned loading functions
    static TinyRig loadMeshSkinned(const std::string& filePath, bool loadSkeleton=true);
};

} // namespace Az3D
