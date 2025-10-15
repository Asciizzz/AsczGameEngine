#pragma once

#include <cstdint>
#include <string>

struct TinyMaterial {
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

    TinyMaterial& setShading(bool enable);
    TinyMaterial& setToonLevel(int level);

    TinyMaterial& setNormalBlend(float blend);
    TinyMaterial& setDiscardThreshold(float threshold);

    // Hash data is required to match textures later
    TinyMaterial& setAlbedoTexture(int texIndex, uint32_t texHash);
    TinyMaterial& setNormalTexture(int texIndex, uint32_t texHash);
};