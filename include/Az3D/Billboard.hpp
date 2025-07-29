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
        glm::vec3 position{0.0f};
        float width = 1.0f;
        float height = 1.0f;
        size_t textureIndex = 0;
        float opacity = 1.0f;  // Alpha multiplier (0.0 = transparent, 1.0 = opaque)
        
        // UV coordinates for sprite sheets (AB1 to AB2)
        glm::vec2 uvMin{0.0f, 0.0f};  // AB1 - top-left UV
        glm::vec2 uvMax{1.0f, 1.0f};  // AB2 - bottom-right UV
        
        Billboard() = default;
        Billboard(const glm::vec3& pos, float w, float h, size_t texIndex, float alpha = 1.0f)
            : position(pos), width(w), height(h), textureIndex(texIndex), opacity(alpha) {}
    };

} // namespace Az3D
