#include "TinyData/TinyVertex.hpp"

#include <vulkan/vulkan.h>

using namespace TinyVertex;

VkVertexInputBindingDescription Layout::bindingDesc() const {
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = stride;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

std::vector<VkVertexInputAttributeDescription> Layout::attributeDescs() const {
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

Static& Static::setPosition(const glm::vec3& position) {
    pos_tu.x = position.x;
    pos_tu.y = position.y;
    pos_tu.z = position.z;
    return *this;
}
Static& Static::setNormal(const glm::vec3& normal) {
    nrml_tv.x = normal.x;
    nrml_tv.y = normal.y;
    nrml_tv.z = normal.z;
    return *this;
}
Static& Static::setTextureUV(const glm::vec2& uv) {
    pos_tu.w  = uv.x;
    nrml_tv.w = uv.y;
    return *this;
}
Static& Static::setTangent(const glm::vec4& tang) {
    tangent = tang;
    return *this;
}

Layout Static::layout() {
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

VkVertexInputBindingDescription Static::bindingDesc() {
    return layout().bindingDesc();
}
std::vector<VkVertexInputAttributeDescription> Static::attributeDescs() {
    return layout().attributeDescs();
}

// Make rigged vertex from static vertex (for debugging)
Rigged Static::makeRigged(const Static& staticVertex) {
    Rigged rigVertex;
    rigVertex.pos_tu = staticVertex.pos_tu;
    rigVertex.nrml_tv = staticVertex.nrml_tv;
    rigVertex.tangent = staticVertex.tangent;
    return rigVertex;
}

std::vector<Rigged> Static::makeRigged(const std::vector<Static>& staticVertices) {
    std::vector<Rigged> rigVertices;
    for (const auto& staticV : staticVertices) {
        rigVertices.push_back(makeRigged(staticV));
    }
    return rigVertices;
}


// Skinning vertex data
Rigged& Rigged::setPosition(const glm::vec3& position) {
    pos_tu.x = position.x;
    pos_tu.y = position.y;
    pos_tu.z = position.z;
    return *this;
}
Rigged& Rigged::setNormal(const glm::vec3& normal) {
    nrml_tv.x = normal.x;
    nrml_tv.y = normal.y;
    nrml_tv.z = normal.z;
    return *this;
}
Rigged& Rigged::setTextureUV(const glm::vec2& uv) {
    pos_tu.w  = uv.x;
    nrml_tv.w = uv.y;
    return *this;
}
Rigged& Rigged::setTangent(const glm::vec4& tang) {
    tangent = tang;
    return *this;
}
Rigged& Rigged::setBoneIDs(const glm::uvec4& ids) {
    boneIDs = ids;
    return *this;
}
Rigged& Rigged::setWeights(const glm::vec4& weights, bool normalize) {
    if (normalize) {
        float total = weights.x + weights.y + weights.z + weights.w;
        if (total > 0.0f) {
            this->weights = weights / total;
            return *this;
        }
    }
    this->weights = weights;
    return *this;
}

Layout Rigged::layout() {
    Layout layout;
    layout.type = Layout::Type::Rigged;
    layout.stride = sizeof(Rigged);
    layout.attributes = {
        {0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Rigged, pos_tu)},
        {1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Rigged, nrml_tv)},
        {2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Rigged, tangent)},
        {3, VK_FORMAT_R32G32B32A32_UINT,   offsetof(Rigged, boneIDs)},
        {4, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Rigged, weights)}
    };
    return layout;
}

VkVertexInputBindingDescription Rigged::bindingDesc() {
    return layout().bindingDesc();
}
std::vector<VkVertexInputAttributeDescription> Rigged::attributeDescs() {
    return layout().attributeDescs();
}


Static Rigged::makeStatic(const Rigged& rigVertex) {
    Static staticVertex;
    staticVertex.pos_tu = rigVertex.pos_tu;
    staticVertex.nrml_tv = rigVertex.nrml_tv;
    staticVertex.tangent = rigVertex.tangent;
    return staticVertex;
}

std::vector<Static> Rigged::makeStatic(const std::vector<Rigged>& rigVertices) {
    std::vector<Static> staticVertices;
    for (const auto& rigV : rigVertices) {
        staticVertices.push_back(makeStatic(rigV));
    }
    return staticVertices;
}