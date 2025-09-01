#pragma once

#include <Az3D/MeshStatic.hpp>
#include <Az3D/MeshSkinned.hpp>


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

class TinyLoader {
public:
    static TinyTexture loadImage(const std::string& filePath);
    
    // MeshStatic loading functions
    static SharedPtr<MeshStatic> loadMeshStatic(const std::string& filePath);
    static SharedPtr<MeshStatic> loadMeshStaticFromOBJ(const std::string& filePath);
    static SharedPtr<MeshStatic> loadMeshStaticFromGLTF(const std::string& filePath);
};

} // namespace Az3D
