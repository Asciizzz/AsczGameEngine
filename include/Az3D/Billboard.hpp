#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>

namespace Az3D {

    struct Billboard {
        glm::vec3 pos{0.0f};
        float width = 1.0f;
        float height = 1.0f;
        size_t textureIndex = 0;
        glm::vec2 uvMin{0.0f, 0.0f};  // AB1 - top-left UV
        glm::vec2 uvMax{1.0f, 1.0f};  // AB2 - bottom-right UV
        glm::vec4 color{1.0f}; // Color multiplier (RGBA)

        Billboard() = default;
        Billboard(const glm::vec3& pos, float w, float h, size_t texIndex, const glm::vec4& color)
            : pos(pos), width(w), height(h), textureIndex(texIndex), color(color) {}
    };

} // namespace Az3D
