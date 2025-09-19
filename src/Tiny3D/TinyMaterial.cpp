#include "TinyMaterial.hpp"

TinyMaterial& TinyMaterial::setShading(bool enable) {
    shading = enable;
    return *this;
}

TinyMaterial& TinyMaterial::setToonLevel(int level) {
    toonLevel = level;
    return *this;
}

TinyMaterial& TinyMaterial::setNormalBlend(float blend) {
    normalBlend = blend;
    return *this;
}

TinyMaterial& TinyMaterial::setDiscardThreshold(float threshold) {
    discardThreshold = threshold;
    return *this;
}

TinyMaterial& TinyMaterial::setAlbedoTexture(int texIndex) {
    albTexture = texIndex;
    return *this;
}

TinyMaterial& TinyMaterial::setNormalTexture(int texIndex) {
    nrmlTexture = texIndex;
    return *this;
}