#pragma once

struct TinyMaterial {
    bool shading = true;
    int toonLevel = 0;

    // Some debug values
    float normalBlend = 0.0f;
    float discardThreshold = 0.01f;

    int albTexture = -1;
    int nrmlTexture = -1;

    TinyMaterial& setShading(bool enable);
    TinyMaterial& setToonLevel(int level);

    TinyMaterial& setNormalBlend(float blend);
    TinyMaterial& setDiscardThreshold(float threshold);

    TinyMaterial& setAlbedoTexture(int texIndex);
    TinyMaterial& setNormalTexture(int texIndex);
};