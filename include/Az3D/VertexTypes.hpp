#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <cstdint>

#include "Helpers/Templates.hpp"

struct VkVertexInputBindingDescription;
struct VkVertexInputAttributeDescription;

namespace Az3D {

// Transform structure
struct Transform {
    glm::vec3 pos{0.0f};
    glm::quat rot{1.0f, 0.0f, 0.0f, 0.0f};
    float scl = 1.0f;

    void translate(const glm::vec3& translation);
    void rotate(const glm::quat& rotation);
    void rotateX(float radians);
    void rotateY(float radians);
    void rotateZ(float radians);
    void scale(float scale);

    glm::mat4 getMat4() const;
    void reset();

    // Useful static methods

    static glm::vec3 rotate(const glm::vec3& point, const glm::vec3& axis, float angle) {
        glm::quat rotation = glm::angleAxis(angle, axis);
        return rotation * point;
    }
};

// Note: 0 handedness for no normal map

// Forward declarations
struct VertexAttribute {
    uint32_t location;
    uint32_t format;    // VkFormat
    uint32_t offset;
};

struct VertexLayout {
    uint32_t stride;
    std::vector<VertexAttribute> attributes;

    // Utility to generate Vulkan descriptions once
    VkVertexInputBindingDescription getBindingDescription() const;
    std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() const;
};

struct StaticVertex {
    // Compact 48 byte data layout

    glm::vec4 pos_tu  = glm::vec4(0.0f); // Position XYZ - Texture U on W
    glm::vec4 nrml_tv = glm::vec4(0.0f); // Normal XYZ - Texture V on W
    glm::vec4 tangent = glm::vec4(0.0f); // Tangent XYZ - Handedness on W

    StaticVertex() = default;
    StaticVertex(const glm::vec3& pos, const glm::vec3& nrml, const glm::vec2& uv, const glm::vec4& tang = glm::vec4(0.0f)) {
        pos_tu = glm::vec4(pos, uv.x);
        nrml_tv = glm::vec4(nrml, uv.y);
        tangent = tang;
    }

    void setPosition(const glm::vec3& position);
    void setNormal(const glm::vec3& normal);
    void setTextureUV(const glm::vec2& uv);

    glm::vec3 getPosition() const { return glm::vec3(pos_tu); }
    glm::vec3 getNormal() const { return glm::vec3(nrml_tv); }
    glm::vec2 getTextureUV() const { return {pos_tu.w, nrml_tv.w}; }

    // Returns layout that can be used for pipeline creation
    static VertexLayout getLayout();
};

struct RigVertex {
    // 80 bytes of data

    glm::vec4 pos_tu = glm::vec4(0.0f);
    glm::vec4 nrml_tv = glm::vec4(0.0f);
    glm::vec4 tangent = glm::vec4(0.0f);

    glm::uvec4 boneIDs = glm::uvec4(0);
    glm::vec4 weights = glm::vec4(0.0f);

    RigVertex() = default;

    // Standard vertex data

    void setPosition(const glm::vec3& position);
    void setNormal(const glm::vec3& normal);
    void setTextureUV(const glm::vec2& uv);

    glm::vec3 getPosition() const { return glm::vec3(pos_tu); }
    glm::vec3 getNormal() const { return glm::vec3(nrml_tv); }
    glm::vec2 getTextureUV() const { return {pos_tu.w, nrml_tv.w}; };

    // Returns layout that can be used for pipeline creation
    static VertexLayout getLayout();
};

} // namespace Az3D