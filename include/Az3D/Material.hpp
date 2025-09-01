#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>

namespace Az3D {

enum class TAddressMode {
    Repeat        = 0,
    ClampToEdge   = 1,
    ClampToBorder = 2
};

struct Material {
    glm::vec4 shadingParams = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); // <bool shading>, <int toonLevel>, <float normalBlend>, <float discardThreshold>
    glm::uvec4 texIndices = glm::uvec4(0, 0, 0, 0); // <albedo>, <normal>, <metallic>, <unsure>

    Material() = default;

    void setShadingParams(bool shading, int toonLevel, float normalBlend, float discardThreshold) {
        shadingParams.x = shading ? 1.0f : 0.0f;
        shadingParams.y = static_cast<float>(toonLevel);
        shadingParams.z = normalBlend;
        shadingParams.w = discardThreshold;
    }

    void setAlbedoTexture(int index, TAddressMode addressMode = TAddressMode::Repeat) {
        texIndices.x = index;
        texIndices.y = static_cast<uint32_t>(addressMode);
    }

    void setNormalTexture(int index, TAddressMode addressMode = TAddressMode::Repeat) {
        texIndices.z = index;
        texIndices.w = static_cast<uint32_t>(addressMode);
    }
};

} // namespace Az3D
