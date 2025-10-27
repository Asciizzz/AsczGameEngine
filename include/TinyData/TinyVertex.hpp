#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>

#include ".ext/Templates.hpp"

struct VkVertexInputBindingDescription;
struct VkVertexInputAttributeDescription;

namespace TinyVertex {

struct Layout {
    uint32_t stride;
    struct Attribute {
        uint32_t location;
        uint32_t format;
        uint32_t offset;
    };
    std::vector<Attribute> attributes;

    enum class Type {
        Static,
        Rigged
    } type = Type::Static;

    // Utility to generate Vulkan descriptions once
    VkVertexInputBindingDescription bindingDesc() const;
    std::vector<VkVertexInputAttributeDescription> attributeDescs() const;
};



struct Rigged; // Forward declaration
struct Static {
    // Compact 48 byte data layout
    // Note: 0 handedness for no normal map

    glm::vec4 pos_tu  = glm::vec4(0.0f); // Position XYZ - Texture U on W
    glm::vec4 nrml_tv = glm::vec4(0.0f); // Normal XYZ - Texture V on W
    glm::vec4 tangent = glm::vec4(0.0f); // Tangent XYZ - Handedness on W

    Static() = default;
    Static(const glm::vec3& pos, const glm::vec3& nrml, const glm::vec2& uv, const glm::vec4& tang = glm::vec4(0.0f)) {
        pos_tu = glm::vec4(pos, uv.x);
        nrml_tv = glm::vec4(nrml, uv.y);
        tangent = tang;
    }

    Static& setPosition(const glm::vec3& position);
    Static& setNormal(const glm::vec3& normal);
    Static& setTextureUV(const glm::vec2& uv);
    Static& setTangent(const glm::vec4& tang);

    glm::vec3 getPosition() const { return glm::vec3(pos_tu); }
    glm::vec3 getNormal() const { return glm::vec3(nrml_tv); }
    glm::vec2 getTextureUV() const { return {pos_tu.w, nrml_tv.w}; }

    // Returns layout that can be used for pipeline creation
    static Layout layout();
    static VkVertexInputBindingDescription bindingDesc();
    static std::vector<VkVertexInputAttributeDescription> attributeDescs();

    // Normally only for debugging
    static Rigged makeRigged(const Static& staticVertex);
    static std::vector<Rigged> makeRigged(const std::vector<Static>& staticVertices);
};

struct Rigged {
    // Compact 80 bytes of data

    glm::vec4 pos_tu = glm::vec4(0.0f);
    glm::vec4 nrml_tv = glm::vec4(0.0f);
    glm::vec4 tangent = glm::vec4(0.0f);

    glm::uvec4 boneIDs = glm::uvec4(0);
    glm::vec4 weights = glm::vec4(0.0f);

    Rigged() = default;

    // Standard vertex data

    Rigged& setPosition(const glm::vec3& position);
    Rigged& setNormal(const glm::vec3& normal);
    Rigged& setTextureUV(const glm::vec2& uv);
    Rigged& setTangent(const glm::vec4& tangent);
    Rigged& setBoneIDs(const glm::uvec4& ids=glm::uvec4(0));
    Rigged& setWeights(const glm::vec4& weights=glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), bool normalize=true);

    glm::vec3 getPosition() const { return glm::vec3(pos_tu); }
    glm::vec3 getNormal() const { return glm::vec3(nrml_tv); }
    glm::vec2 getTextureUV() const { return {pos_tu.w, nrml_tv.w}; };

    // Returns layout that can be used for pipeline creation
    static Layout layout();
    static VkVertexInputBindingDescription bindingDesc();
    static std::vector<VkVertexInputAttributeDescription> attributeDescs();

    static Static makeStatic(const Rigged& rigVertex);
    static std::vector<Static> makeStatic(const std::vector<Rigged>& rigVertices);
};

} // namespace TinyVertex