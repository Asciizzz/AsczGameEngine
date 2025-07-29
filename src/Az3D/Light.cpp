#include "Az3D/Light.hpp"

namespace Az3D {
    LightManager::LightManager() {
        // Initialize with a default directional light
        setDirectionalLight(
            glm::vec3(0.2f, -1.0f, 0.3f),  // Direction (slightly down and to the side)
            glm::vec3(1.0f, 0.95f, 0.8f),  // Warm sunlight color
            1.0f                           // Full intensity
        );
    }

    void LightManager::setDirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity) {
        directionalLight.direction = glm::normalize(direction);
        directionalLight.color = color;
        directionalLight.intensity = intensity;
        directionalLight.padding1 = 0.0f;
        
        // Initialize light view projection matrix (identity for now, will be used for shadow mapping)
        directionalLight.lightViewProj = glm::mat4(1.0f);
    }

    void LightManager::updateDirectionalLightDirection(const glm::vec3& direction) {
        directionalLight.direction = glm::normalize(direction);
    }

    void LightManager::updateDirectionalLightColor(const glm::vec3& color) {
        directionalLight.color = color;
    }

    void LightManager::updateDirectionalLightIntensity(float intensity) {
        directionalLight.intensity = intensity;
    }

    void LightManager::updateLightViewProj(const glm::mat4& lightViewProj) {
        directionalLight.lightViewProj = lightViewProj;
    }
}
