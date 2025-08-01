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
        glm::vec3 position{0.0f};  // Changed from pos to position
        float width = 1.0f;
        float height = 1.0f;
        size_t textureIndex = 0;
        glm::vec2 uvMin{0.0f, 0.0f};  // AB1 - top-left UV
        glm::vec2 uvMax{1.0f, 1.0f};  // AB2 - bottom-right UV
        float opacity = 1.0f;         // Alpha multiplier for transparency
        glm::vec4 color{1.0f};        // Color multiplier (RGBA) - legacy

        Billboard() = default;
        Billboard(const glm::vec3& position, float w, float h, size_t texIndex, float opacity = 1.0f)
            : position(position), width(w), height(h), textureIndex(texIndex), opacity(opacity) {}
        // Legacy color constructor for compatibility
        Billboard(const glm::vec3& position, float w, float h, size_t texIndex, const glm::vec4& color)
            : position(position), width(w), height(h), textureIndex(texIndex), color(color) {}
        // Same thing but with size for both width and height
        Billboard(const glm::vec3& position, float size, size_t texIndex, float opacity = 1.0f)
            : position(position), width(size), height(size), textureIndex(texIndex), opacity(opacity) {}
    };

} // namespace Az3D
