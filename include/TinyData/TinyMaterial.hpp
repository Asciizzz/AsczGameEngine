#pragma once

#include <cstdint>
#include <string>

struct tinyMaterial {
    std::string name; // Material name from glTF
    
    bool shading = true;
    int toonLevel = 0;

    // Some debug values
    float normalBlend = 0.0f;
    float discardThreshold = 0.01f;

    int localAlbTexture = -1;
    int localNrmlTexture = -1;

    uint32_t albTexHash = 0;
    uint32_t nrmlTexHash = 0;

    tinyMaterial& setShading(bool enable);
    tinyMaterial& setToonLevel(int level);

    tinyMaterial& setNrmlBlend(float blend);
    tinyMaterial& setDiscardThreshold(float threshold);

    // Hash data is required to match textures later
    tinyMaterial& setAlbedoTexture(int texIndex, uint32_t texHash);
    tinyMaterial& setNrmlTexture(int texIndex, uint32_t texHash);
};