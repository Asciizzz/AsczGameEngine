#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Helpers/Templates.hpp"

struct VkVertexInputBindingDescription;
struct VkVertexInputAttributeDescription;

namespace Az3D {

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

// Note: 0 handedness for no normal map


struct VertexStatic {
    // Compact 48 byte data layout

    // Position on XYZ and Texture U on W
    glm::vec4 pos_tu = glm::vec4(0.0f);
    // Normal on XYZ and Texture V on W
    glm::vec4 nrml_tv = glm::vec4(0.0f);
    // Tangent XYZ and handedness on W
    glm::vec4 tangent = glm::vec4(0.0f);

    VertexStatic() = default;
    VertexStatic(const glm::vec3& pos, const glm::vec3& nrml, const glm::vec2& uv) {
        pos_tu = glm::vec4(pos, uv.x);
        nrml_tv = glm::vec4(nrml, uv.y);
    }

    void setPosition(const glm::vec3& position);
    void setNormal(const glm::vec3& normal);
    void setTextureUV(const glm::vec2& uv);

    glm::vec3 getPosition() const { return glm::vec3(pos_tu); }
    glm::vec3 getNormal() const { return glm::vec3(nrml_tv); }
    glm::vec2 getTextureUV() const { return {pos_tu.w, nrml_tv.w}; }

    // Vulkan binding description for rendering
    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
};

struct VertexSkinned {
    // 80 bytes of data

    glm::vec4 pos_tu = glm::vec4(0.0f);
    glm::vec4 nrml_tv = glm::vec4(0.0f);
    glm::vec4 tangent = glm::vec4(0.0f);

    glm::uvec4 boneIDs = glm::uvec4(0);
    glm::vec4 weights = glm::vec4(0.0f);

    VertexSkinned() = default;

    // Standard vertex data

    void setPosition(const glm::vec3& position);
    void setNormal(const glm::vec3& normal);
    void setTextureUV(const glm::vec2& uv);

    glm::vec3 getPosition() const { return glm::vec3(pos_tu); }
    glm::vec3 getNormal() const { return glm::vec3(nrml_tv); }
    glm::vec2 getTextureUV() const { return {pos_tu.w, nrml_tv.w}; };

    // Vulkan binding description for rendering
    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions();
};

}