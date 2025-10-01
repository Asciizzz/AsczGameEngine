#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>

#include ".ext/Templates.hpp"

struct VkVertexInputBindingDescription;
struct VkVertexInputAttributeDescription;

struct TinyVertexLayout {
    uint32_t stride;
    struct Attribute {
        uint32_t location;
        uint32_t format;
        uint32_t offset;
    };
    std::vector<Attribute> attributes;

    // Utility to generate Vulkan descriptions once
    VkVertexInputBindingDescription getBindingDescription() const;
    std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() const;
};



struct TinyVertexStatic {
    // Compact 48 byte data layout
    // Note: 0 handedness for no normal map

    glm::vec4 pos_tu  = glm::vec4(0.0f); // Position XYZ - Texture U on W
    glm::vec4 nrml_tv = glm::vec4(0.0f); // Normal XYZ - Texture V on W
    glm::vec4 tangent = glm::vec4(0.0f); // Tangent XYZ - Handedness on W

    TinyVertexStatic() = default;
    TinyVertexStatic(const glm::vec3& pos, const glm::vec3& nrml, const glm::vec2& uv, const glm::vec4& tang = glm::vec4(0.0f)) {
        pos_tu = glm::vec4(pos, uv.x);
        nrml_tv = glm::vec4(nrml, uv.y);
        tangent = tang;
    }

    TinyVertexStatic& setPosition(const glm::vec3& position);
    TinyVertexStatic& setNormal(const glm::vec3& normal);
    TinyVertexStatic& setTextureUV(const glm::vec2& uv);
    TinyVertexStatic& setTangent(const glm::vec4& tang);

    glm::vec3 getPosition() const { return glm::vec3(pos_tu); }
    glm::vec3 getNormal() const { return glm::vec3(nrml_tv); }
    glm::vec2 getTextureUV() const { return {pos_tu.w, nrml_tv.w}; }

    // Returns layout that can be used for pipeline creation
    static TinyVertexLayout getLayout();
    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

struct TinyVertexRig {
    // Compact 80 bytes of data

    glm::vec4 pos_tu = glm::vec4(0.0f);
    glm::vec4 nrml_tv = glm::vec4(0.0f);
    glm::vec4 tangent = glm::vec4(0.0f);

    glm::uvec4 boneIDs = glm::uvec4(0);
    glm::vec4 weights = glm::vec4(0.0f);

    TinyVertexRig() = default;

    // Standard vertex data

    TinyVertexRig& setPosition(const glm::vec3& position);
    TinyVertexRig& setNormal(const glm::vec3& normal);
    TinyVertexRig& setTextureUV(const glm::vec2& uv);
    TinyVertexRig& setTangent(const glm::vec4& tangent);
    TinyVertexRig& setBoneIDs(const glm::uvec4& ids=glm::uvec4(0));
    TinyVertexRig& setWeights(const glm::vec4& weights=glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), bool normalize=true);

    glm::vec3 getPosition() const { return glm::vec3(pos_tu); }
    glm::vec3 getNormal() const { return glm::vec3(nrml_tv); }
    glm::vec2 getTextureUV() const { return {pos_tu.w, nrml_tv.w}; };

    // Returns layout that can be used for pipeline creation
    static TinyVertexLayout getLayout();
    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

    static TinyVertexStatic makeStaticVertex(const TinyVertexRig& rigVertex);
    static std::vector<TinyVertexStatic> makeStaticVertices(const std::vector<TinyVertexRig>& rigVertices);
};