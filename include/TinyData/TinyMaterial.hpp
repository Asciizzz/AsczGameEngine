#pragma once

#include "tinyExt/tinyHandle.hpp"
#include "tinyData/tinyTexture.hpp"

struct tinyMaterialVk {
    std::string name; // Material name from glTF

    tinyHandle albHandle; // Albedo texture
    tinyHandle nrmlHandle; // Normal texture
};