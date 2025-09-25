#include "TinyData/TinyMaterial.hpp"

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

TinyMaterial& TinyMaterial::setAlbedoTexture(int texIndex, uint32_t texHash) {
    localAlbTexture = texIndex;
    albTexHash = texHash;
    return *this;
}

TinyMaterial& TinyMaterial::setNormalTexture(int texIndex, uint32_t texHash) {
    localNrmlTexture = texIndex;
    nrmlTexHash = texHash;
    return *this;
}