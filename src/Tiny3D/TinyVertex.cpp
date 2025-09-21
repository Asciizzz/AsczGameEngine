#include "Tiny3D/TinyVertex.hpp"

#include <vulkan/vulkan.h>

VkVertexInputBindingDescription TinyVertexLayout::getBindingDescription() const {
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = stride;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

std::vector<VkVertexInputAttributeDescription> TinyVertexLayout::getAttributeDescriptions() const {
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

// Static Vertex implementation

TinyVertexStatic& TinyVertexStatic::setPosition(const glm::vec3& position) {
    pos_tu.x = position.x;
    pos_tu.y = position.y;
    pos_tu.z = position.z;
    return *this;
}
TinyVertexStatic& TinyVertexStatic::setNormal(const glm::vec3& normal) {
    nrml_tv.x = normal.x;
    nrml_tv.y = normal.y;
    nrml_tv.z = normal.z;
    return *this;
}
TinyVertexStatic& TinyVertexStatic::setTextureUV(const glm::vec2& uv) {
    pos_tu.w  = uv.x;
    nrml_tv.w = uv.y;
    return *this;
}
TinyVertexStatic& TinyVertexStatic::setTangent(const glm::vec4& tang) {
    tangent = tang;
    return *this;
}

TinyVertexLayout TinyVertexStatic::getLayout() {
    TinyVertexLayout layout;
    layout.stride = sizeof(TinyVertexStatic);
    layout.attributes = {
        {0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(TinyVertexStatic, pos_tu)},
        {1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(TinyVertexStatic, nrml_tv)},
        {2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(TinyVertexStatic, tangent)}
    };
    return layout;
}

VkVertexInputBindingDescription TinyVertexStatic::getBindingDescription() {
    return getLayout().getBindingDescription();
}
std::vector<VkVertexInputAttributeDescription> TinyVertexStatic::getAttributeDescriptions() {
    return getLayout().getAttributeDescriptions();
}


// Skinning vertex data
TinyVertexRig& TinyVertexRig::setPosition(const glm::vec3& position) {
    pos_tu.x = position.x;
    pos_tu.y = position.y;
    pos_tu.z = position.z;
    return *this;
}
TinyVertexRig& TinyVertexRig::setNormal(const glm::vec3& normal) {
    nrml_tv.x = normal.x;
    nrml_tv.y = normal.y;
    nrml_tv.z = normal.z;
    return *this;
}
TinyVertexRig& TinyVertexRig::setTextureUV(const glm::vec2& uv) {
    pos_tu.w  = uv.x;
    nrml_tv.w = uv.y;
    return *this;
}
TinyVertexRig& TinyVertexRig::setTangent(const glm::vec4& tang) {
    tangent = tang;
    return *this;
}
TinyVertexRig& TinyVertexRig::setBoneIDs(const glm::uvec4& ids) {
    boneIDs = ids;
    return *this;
}
TinyVertexRig& TinyVertexRig::setWeights(const glm::vec4& weights) {
    this->weights = weights;
    return *this;
}

TinyVertexLayout TinyVertexRig::getLayout() {
    TinyVertexLayout layout;
    layout.stride = sizeof(TinyVertexRig);
    layout.attributes = {
        {0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(TinyVertexRig, pos_tu)},
        {1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(TinyVertexRig, nrml_tv)},
        {2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(TinyVertexRig, tangent)},
        {3, VK_FORMAT_R32G32B32A32_UINT,   offsetof(TinyVertexRig, boneIDs)},
        {4, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(TinyVertexRig, weights)}
    };
    return layout;
}

VkVertexInputBindingDescription TinyVertexRig::getBindingDescription() {
    return getLayout().getBindingDescription();
}
std::vector<VkVertexInputAttributeDescription> TinyVertexRig::getAttributeDescriptions() {
    return getLayout().getAttributeDescriptions();
}