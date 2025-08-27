#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Helpers/Templates.hpp"

struct VkVertexInputBindingDescription;
struct VkVertexInputAttributeDescription;

namespace Az3D {

struct Vertex {
    glm::vec3 pos;
    glm::vec3 nrml;
    glm::vec2 txtr;

    Vertex() : pos(0.0f), nrml(0.0f), txtr(0.0f) {}
    Vertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& texCoord)
        : pos(position), nrml(normal), txtr(texCoord) {}

    // Vulkan binding description for rendering
    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
};

// Transform structure
struct Transform {
    glm::vec3 pos{0.0f};
    glm::quat rot{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scl{1.0f};

    void translate(const glm::vec3& translation);
    void rotate(const glm::quat& rotation);
    void rotateX(float radians);
    void rotateY(float radians);
    void rotateZ(float radians);
    void scale(float scale);
    void scale(const glm::vec3& scale);

    glm::mat4 getMat4() const;
    void reset();

    // Useful static methods

    static glm::vec3 rotate(const glm::vec3& point, const glm::vec3& axis, float angle) {
        glm::quat rotation = glm::angleAxis(angle, axis);
        return rotation * point;
    }
};

}