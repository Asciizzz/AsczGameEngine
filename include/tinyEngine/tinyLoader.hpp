#pragma once

#include "tinyModel.hpp"

class tinyLoader {
public:
    static tinyTexture loadTexture(const std::string& filePath);

    static tinyModel loadModel(const std::string& filePath, bool forceStatic = false);
private:
    static tinyModel loadModelFromGLTF(const std::string& filePath, bool forceStatic = false);
    static tinyModel loadModelFromOBJ(const std::string& filePath);
};
