#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Az3D {

    struct DirectionalLight {
        alignas(16) glm::vec3 direction;
        float padding1;
        alignas(16) glm::vec3 color;  
        float intensity;
        alignas(16) glm::mat4 lightViewProj; // For shadow mapping (future use)
    };

    class LightManager {
    public:
        LightManager() = default;
        LightManager(const LightManager&) = delete;
        LightManager& operator=(const LightManager&) = delete;

        // Directional light management
        void setDirectionalLight(const glm::vec3& direction, const glm::vec3& color = glm::vec3(1.0f), float intensity = 1.0f);
        void updateDirectionalLightDirection(const glm::vec3& direction);
        void updateDirectionalLightColor(const glm::vec3& color);
        void updateDirectionalLightIntensity(float intensity);

        // Future: Shadow mapping support
        void updateLightViewProj(const glm::mat4& lightViewProj);

        DirectionalLight directionalLight;
    };
}
