#include "Az3D/Light.hpp"

namespace Az3D {



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
