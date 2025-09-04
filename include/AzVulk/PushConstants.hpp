#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace AzVulk {

// Common push constant structures
struct BasicPushConstants {
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    glm::vec4 color = glm::vec4(1.0f);
};

struct MaterialPushConstants {
    int materialIndex = 0;
    int textureIndex = 0;
    float metallic = 0.0f;
    float roughness = 1.0f;
};

struct LightingPushConstants {
    glm::vec3 lightPosition = glm::vec3(0.0f);
    float lightIntensity = 1.0f;
    glm::vec3 lightColor = glm::vec3(1.0f);
    float padding = 0.0f;
};

// Helper functions to create push constant ranges
inline VkPushConstantRange createPushConstantRange(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size) {
    VkPushConstantRange range{};
    range.stageFlags = stageFlags;
    range.offset = offset;
    range.size = size;
    return range;
}

template<typename T>
inline VkPushConstantRange createPushConstantRange(VkShaderStageFlags stageFlags, uint32_t offset = 0) {
    return createPushConstantRange(stageFlags, offset, sizeof(T));
}

} // namespace AzVulk
