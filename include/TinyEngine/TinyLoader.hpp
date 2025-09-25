#pragma once

#include "TinyData/TinyModel.hpp"

class TinyLoader {
public:
    static TinyTexture loadTexture(const std::string& filePath);

    // True implementation that we will use in the future
    static TinyModel loadModel(const std::string& filePath, bool forceStatic = false);

private:
    static TinyModel loadModelFromGLTF(const std::string& filePath, bool forceStatic);
    static TinyModel loadModelFromOBJ(const std::string& filePath, bool forceStatic);

    static std::string sanitizeAsciiz(const std::string& originalName, const std::string& key, size_t fallbackIndex = 0);


    // New modern implementation that act like templates/prefabs
    static TinyModelNew loadModelFromGLTFNew(const std::string& filePath, bool forceStatic);
};
