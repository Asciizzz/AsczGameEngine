#include "Az3D/VertexTypes.hpp"

#include <vulkan/vulkan.h>

namespace Az3D {

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

void Transform::scale(const glm::vec3& scale) {
    this->scl *= scale;
}

glm::mat4 Transform::getMat4() const {
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), pos);
    glm::mat4 rotMat      = glm::mat4_cast(rot);
    glm::mat4 scaleMat    = glm::scale(glm::mat4(1.0f), scl);

    return translation * rotMat * scaleMat;
}

void Transform::reset() {
    pos = glm::vec3(0.0f);
    rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    scl = glm::vec3(1.0f);
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

VkVertexInputBindingDescription VertexStatic::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof(VertexStatic);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 3> VertexStatic::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attribs{};

    attribs[0] = {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexStatic, pos_tu)};
    attribs[1] = {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexStatic, nrml_tv)};
    attribs[2] = {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexStatic, tangent)};

    return attribs;
}



// Skinning vertex data
void VertexSkinned::setPosition(const glm::vec3& position) {
    pos_tu.x = position.x;
    pos_tu.y = position.y;
    pos_tu.z = position.z;
}
void VertexSkinned::setNormal(const glm::vec3& normal) {
    nrml_tv.x = normal.x;
    nrml_tv.y = normal.y;
    nrml_tv.z = normal.z;
}
void VertexSkinned::setTextureUV(const glm::vec2& uv) {
    pos_tu.w  = uv.x;
    nrml_tv.w = uv.y;
}

VkVertexInputBindingDescription VertexSkinned::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof(VertexSkinned);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 5> VertexSkinned::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 5> attribs{};

    attribs[0] = {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexSkinned, pos_tu)};
    attribs[1] = {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexSkinned, nrml_tv)};
    attribs[2] = {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexSkinned, tangent)};
    attribs[3] = {3, 0, VK_FORMAT_R32G32B32A32_UINT,   offsetof(VertexSkinned, boneIDs)};
    attribs[4] = {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexSkinned, weights)};

    return attribs;
}

} // namespace Az3D