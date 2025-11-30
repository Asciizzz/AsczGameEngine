#pragma once

#include <glm/glm.hpp>
#include "tinyType.hpp"

struct  tinyMaterial {
    struct alignas(16) Data { // Pure data, shader determines context
        glm::vec4 float1 = glm::vec4(1.0f);
        glm::vec4 float2 = glm::vec4(0.0f);
        glm::vec4 float3 = glm::vec4(0.0f);
        glm::vec4 float4 = glm::vec4(0.0f);
        glm::uvec4 uint1 = glm::uvec4(0);
        glm::uvec4 uint2 = glm::uvec4(0);
    } data; // Total size: 96 bytes

    tinyHandle shader;

    // These all we need for now
    glm::vec4 baseColor;
    tinyHandle albTexture;
    tinyHandle nrmlTexture;
}; 