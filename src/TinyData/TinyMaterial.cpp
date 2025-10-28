#include "tinyData/tinyMaterial.hpp"

tinyMaterial& tinyMaterial::setShading(bool enable) {
    shading = enable;
    return *this;
}

tinyMaterial& tinyMaterial::setToonLevel(int level) {
    toonLevel = level;
    return *this;
}

tinyMaterial& tinyMaterial::setNormalBlend(float blend) {
    normalBlend = blend;
    return *this;
}

tinyMaterial& tinyMaterial::setDiscardThreshold(float threshold) {
    discardThreshold = threshold;
    return *this;
}

tinyMaterial& tinyMaterial::setAlbedoTexture(int texIndex, uint32_t texHash) {
    localAlbTexture = texIndex;
    albTexHash = texHash;
    return *this;
}

tinyMaterial& tinyMaterial::setNormalTexture(int texIndex, uint32_t texHash) {
    localNrmlTexture = texIndex;
    nrmlTexHash = texHash;
    return *this;
}