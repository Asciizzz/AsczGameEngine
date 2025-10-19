#pragma once

#include "TinyData/TinyModel.hpp"

class TinyLoader {
public:
    static TinyTexture loadTexture(const std::string& filePath);

    static TinyModel loadModel(const std::string& filePath, bool forceStatic = false);
    
    static std::string sanitizeAsciiz(const std::string& originalName, const std::string& key, size_t fallbackIndex = 0);

private:
    static TinyModel loadModelFromGLTF(const std::string& filePath, bool forceStatic = false);

    static TinyModel loadModelFromOBJ(const std::string& filePath);
};
