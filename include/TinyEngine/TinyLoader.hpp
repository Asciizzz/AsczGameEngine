#pragma once

#include "TinyData/TinyModel.hpp"

class TinyLoader {
public:
    static TinyTexture loadTexture(const std::string& filePath);

    static TinyModel loadModel(const std::string& filePath, bool forceStatic = false);
    
    static std::string sanitizeAsciiz(const std::string& originalName, const std::string& key, size_t fallbackIndex = 0);

private:
    // Not recommended for obsolete reasons, use gltf or fbx(future) instead
    static TinyModel loadModelFromOBJ(const std::string& filePath, bool forceStatic);

    static TinyModel loadModelFromGLTF(const std::string& filePath, bool forceStatic = false);
};
