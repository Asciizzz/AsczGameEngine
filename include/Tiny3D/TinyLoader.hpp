#pragma once

#include "Tiny3D/TinyModel.hpp"

class TinyLoader {
public:
    struct LoadOptions {
        bool loadMaterials = true;
        bool loadTextures = true;
        bool forceStatic = false; // Ignore skinning data even if present
    };

    static TinyTexture loadImage(const std::string& filePath);

    // True implementation that we will use in the future
    static TinyModel loadModel(const std::string& filePath, const LoadOptions& options = LoadOptions());

private:
    static TinyModel loadModelFromGLTF(const std::string& filePath, const LoadOptions& options);
    static TinyModel loadModelFromOBJ(const std::string& filePath, const LoadOptions& options);

    static std::string sanitizeAsciiz(const std::string& originalName, const std::string& key, size_t fallbackIndex = 0);
};
