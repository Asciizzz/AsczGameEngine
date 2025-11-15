#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vulkan/vulkan.h>

#include "Templates.hpp"

namespace tinyVertex {

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
    VkVertexInputBindingDescription bindingDesc() const noexcept {
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = stride;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding;
    }

    std::vector<VkVertexInputAttributeDescription> attributeDescs() const noexcept {
        std::vector<VkVertexInputAttributeDescription> descs;
        for (const auto& attr : attributes) {
            VkVertexInputAttributeDescription d{};
            d.binding = 0;
            d.location = attr.location;
            d.format = static_cast<VkFormat>(attr.format);
            d.offset = attr.offset;
            descs.push_back(d);
        }
        return descs;
    }
};


struct Static {
    // Compact 48 byte data layout
    // Note: 0 handedness for no normal map

    glm::vec4 pos_tu  = glm::vec4(0.0f); // Position XYZ - Texture U on W
    glm::vec4 nrml_tv = glm::vec4(0.0f); // Normal XYZ - Texture V on W
    glm::vec4 tangent = glm::vec4(0.0f); // Tangent XYZ - Handedness on W

    Static() noexcept = default;
    Static(const glm::vec3& pos, const glm::vec3& nrml, const glm::vec2& uv, const glm::vec4& tang = glm::vec4(0.0f)) {
        pos_tu = glm::vec4(pos, uv.x);
        nrml_tv = glm::vec4(nrml, uv.y);
        tangent = tang;
    }

    Static& setPos(const glm::vec3& position) noexcept {
        pos_tu.x = position.x;
        pos_tu.y = position.y;
        pos_tu.z = position.z;
        return *this;
    }

    Static& setNrml(const glm::vec3& normal) noexcept {
        nrml_tv.x = normal.x;
        nrml_tv.y = normal.y;
        nrml_tv.z = normal.z;
        return *this;
    }

    Static& setUV(const glm::vec2& uv) noexcept {
        pos_tu.w  = uv.x;
        nrml_tv.w = uv.y;
        return *this;
    }

    Static& setTang(const glm::vec4& tang) noexcept {
        tangent = tang;
        return *this;
    }

    glm::vec3 getPos() const noexcept { return glm::vec3(pos_tu); }
    glm::vec3 getNrml() const noexcept { return glm::vec3(nrml_tv); }
    glm::vec2 getUV() const noexcept { return {pos_tu.w, nrml_tv.w}; }

    // Returns layout that can be used for pipeline creation
    static Layout layout() noexcept {
        Layout layout;
        layout.type = Layout::Type::Static;
        layout.stride = sizeof(Static);
        layout.attributes = {
            {0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Static, pos_tu)},
            {1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Static, nrml_tv)},
            {2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Static, tangent)}
        };
        return layout;
    }

    static VkVertexInputBindingDescription bindingDesc() noexcept {
        return layout().bindingDesc();
    }
    static std::vector<VkVertexInputAttributeDescription> attributeDescs() noexcept {
        return layout().attributeDescs();
    }
};

struct Rigged {
    // Compact 80 bytes of data

    glm::vec4 pos_tu = glm::vec4(0.0f);
    glm::vec4 nrml_tv = glm::vec4(0.0f);
    glm::vec4 tangent = glm::vec4(0.0f);

    glm::uvec4 boneIDs = glm::uvec4(0);
    glm::vec4 boneWs = glm::vec4(0.0f);

    Rigged() noexcept = default;

    // Standard vertex data

    Rigged& setPos(const glm::vec3& position) noexcept {
        pos_tu.x = position.x;
        pos_tu.y = position.y;
        pos_tu.z = position.z;
        return *this;
    }

    Rigged& setNrml(const glm::vec3& normal) noexcept {
        nrml_tv.x = normal.x;
        nrml_tv.y = normal.y;
        nrml_tv.z = normal.z;
        return *this;
    }

    Rigged& setUV(const glm::vec2& uv) noexcept {
        pos_tu.w  = uv.x;
        nrml_tv.w = uv.y;
        return *this;
    }

    Rigged& setTang(const glm::vec4& tang) noexcept {
        tangent = tang;
        return *this;
    }

    Rigged& setBoneIDs(const glm::uvec4& ids=glm::uvec4(0)) noexcept {
        boneIDs = ids;
        return *this;
    }

    Rigged& setBoneWs(const glm::vec4& ws=glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), bool normalize=true) noexcept {
        boneWs = normalize ? glm::normalize(ws) : ws;
        return *this;
    }

    glm::vec3 getPos() const noexcept { return glm::vec3(pos_tu); }
    glm::vec3 getNrml() const noexcept { return glm::vec3(nrml_tv); }
    glm::vec2 getUV() const noexcept { return {pos_tu.w, nrml_tv.w}; };

    // Returns layout that can be used for pipeline creation
    static Layout layout() noexcept {
        Layout layout;
        layout.type = Layout::Type::Rigged;
        layout.stride = sizeof(Rigged);
        layout.attributes = {
            {0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Rigged, pos_tu)},
            {1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Rigged, nrml_tv)},
            {2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Rigged, tangent)},
            {3, VK_FORMAT_R32G32B32A32_UINT,   offsetof(Rigged, boneIDs)},
            {4, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Rigged, boneWs)}
        };
        return layout;
    }

    static VkVertexInputBindingDescription bindingDesc() noexcept {
        return layout().bindingDesc();
    }
    static std::vector<VkVertexInputAttributeDescription> attributeDescs() noexcept {
        return layout().attributeDescs();
    }

    static Static makeStatic(const Rigged& rigVertex) noexcept {
        Static staticVertex;
        staticVertex.pos_tu = rigVertex.pos_tu;
        staticVertex.nrml_tv = rigVertex.nrml_tv;
        staticVertex.tangent = rigVertex.tangent;
        return staticVertex;
    }

    static std::vector<Static> makeStatic(const std::vector<Rigged>& rigVertices) noexcept {
        std::vector<Static> staticVertices;
        staticVertices.reserve(rigVertices.size());
        for (const auto& rigV : rigVertices) {
            staticVertices.push_back(makeStatic(rigV));
        }
        return staticVertices;
    }
};

} // namespace tinyVertex