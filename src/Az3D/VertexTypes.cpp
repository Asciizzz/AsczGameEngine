#include "Az3D/VertexTypes.hpp"

#include <vulkan/vulkan.h>

using namespace Az3D;

// VertexLayout implementation
VkVertexInputBindingDescription VertexLayout::getBindingDescription() const {
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = stride;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

std::vector<VkVertexInputAttributeDescription> VertexLayout::getAttributeDescriptions() const {
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

void Transform::translate(const glm::vec3& translation) {
    this->pos += translation;
}

void Transform::rotate(const glm::quat& quaternion) {
    this->rot = quaternion * this->rot; // Multiply quaternion rots
}

void Transform::rotateX(float radians) {
    glm::quat xrot = glm::angleAxis(radians, glm::vec3(1.0f, 0.0f, 0.0f));
    this->rot = xrot * this->rot;
}

void Transform::rotateY(float radians) {
    glm::quat yrot = glm::angleAxis(radians, glm::vec3(0.0f, 1.0f, 0.0f));
    this->rot = yrot * this->rot;
}

void Transform::rotateZ(float radians) {
    glm::quat zrot = glm::angleAxis(radians, glm::vec3(0.0f, 0.0f, 1.0f));
    this->rot = zrot * this->rot;
}

void Transform::scale(float scale) {
    this->scl *= scale;
}

glm::mat4 Transform::getMat4() const {
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), pos);
    glm::mat4 rotMat      = glm::mat4_cast(rot);
    glm::mat4 scaleMat    = glm::scale(glm::mat4(1.0f), glm::vec3(scl));

    return translation * rotMat * scaleMat;
}

void Transform::reset() {
    pos = glm::vec3(0.0f);
    rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    scl = 1.0f;
}

// Static Vertex implementation

void VertexStatic::setPosition(const glm::vec3& position) {
    pos_tu.x = position.x;
    pos_tu.y = position.y;
    pos_tu.z = position.z;
}
void VertexStatic::setNormal(const glm::vec3& normal) {
    nrml_tv.x = normal.x;
    nrml_tv.y = normal.y;
    nrml_tv.z = normal.z;
}
void VertexStatic::setTextureUV(const glm::vec2& uv) {
    pos_tu.w  = uv.x;
    nrml_tv.w = uv.y;
}

VertexLayout VertexStatic::getLayout() {
    VertexLayout layout;
    layout.stride = sizeof(VertexStatic);
    layout.attributes = {
        {0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexStatic, pos_tu)},
        {1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexStatic, nrml_tv)},
        {2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexStatic, tangent)}
    };
    return layout;
}



// Skinning vertex data
void VertexRig::setPosition(const glm::vec3& position) {
    pos_tu.x = position.x;
    pos_tu.y = position.y;
    pos_tu.z = position.z;
}
void VertexRig::setNormal(const glm::vec3& normal) {
    nrml_tv.x = normal.x;
    nrml_tv.y = normal.y;
    nrml_tv.z = normal.z;
}
void VertexRig::setTextureUV(const glm::vec2& uv) {
    pos_tu.w  = uv.x;
    nrml_tv.w = uv.y;
}

VertexLayout VertexRig::getLayout() {
    VertexLayout layout;
    layout.stride = sizeof(VertexRig);
    layout.attributes = {
        {0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexRig, pos_tu)},
        {1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexRig, nrml_tv)},
        {2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexRig, tangent)},
        {3, VK_FORMAT_R32G32B32A32_UINT,   offsetof(VertexRig, boneIDs)},
        {4, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexRig, weights)}
    };
    return layout;
}