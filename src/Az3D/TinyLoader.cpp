#include "Az3D/TinyLoader.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "Helpers/stb_image.h"

using namespace Az3D;

void TinyTexture::free() {
    if (data) {
        stbi_image_free(data);
        data = nullptr;
    }
}

TinyTexture TinyLoader::loadImage(const std::string& filePath) {
    TinyTexture texture = {};
    texture.data = stbi_load(
        filePath.c_str(),
        &texture.width,
        &texture.height,
        &texture.channels,
        STBI_rgb_alpha
    );
    
    // Check if loading failed
    if (!texture.data) {
        // Reset dimensions if loading failed
        texture.width = 0;
        texture.height = 0;
        texture.channels = 0;
    }
    
    return texture;
}