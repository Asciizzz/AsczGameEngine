#pragma once

#include "tinyData/tinyModel.hpp"

class tinyLoader {
public:
    static tinyTexture loadTexture(const std::string& filePath);

    static tinyModel loadModel(const std::string& filePath, bool forceStatic = false);
    
    static std::string sanitizeAsciiz(const std::string& originalName, const std::string& key, size_t fallbackIndex = 0);

private:
    static tinyModel loadModelFromGLTF(const std::string& filePath, bool forceStatic = false);

    static tinyModel loadModelFromOBJ(const std::string& filePath);
};
