#include "Az3D/VertexTypes.hpp"

#include <vulkan/vulkan.h>

using namespace Az3D;

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

void StaticVertex::setPosition(const glm::vec3& position) {
    pos_tu.x = position.x;
    pos_tu.y = position.y;
    pos_tu.z = position.z;
}
void StaticVertex::setNormal(const glm::vec3& normal) {
    nrml_tv.x = normal.x;
    nrml_tv.y = normal.y;
    nrml_tv.z = normal.z;
}
void StaticVertex::setTextureUV(const glm::vec2& uv) {
    pos_tu.w  = uv.x;
    nrml_tv.w = uv.y;
}

VkVertexInputBindingDescription StaticVertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof(StaticVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> StaticVertex::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attribs(3);

    attribs[0] = {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(StaticVertex, pos_tu)};
    attribs[1] = {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(StaticVertex, nrml_tv)};
    attribs[2] = {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(StaticVertex, tangent)};

    return attribs;
}



// Skinning vertex data
void RigVertex::setPosition(const glm::vec3& position) {
    pos_tu.x = position.x;
    pos_tu.y = position.y;
    pos_tu.z = position.z;
}
void RigVertex::setNormal(const glm::vec3& normal) {
    nrml_tv.x = normal.x;
    nrml_tv.y = normal.y;
    nrml_tv.z = normal.z;
}
void RigVertex::setTextureUV(const glm::vec2& uv) {
    pos_tu.w  = uv.x;
    nrml_tv.w = uv.y;
}

VkVertexInputBindingDescription RigVertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof(RigVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> RigVertex::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attribs(5);

    attribs[0] = {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(RigVertex, pos_tu)};
    attribs[1] = {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(RigVertex, nrml_tv)};
    attribs[2] = {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(RigVertex, tangent)};
    attribs[3] = {3, 0, VK_FORMAT_R32G32B32A32_UINT,   offsetof(RigVertex, boneIDs)};
    attribs[4] = {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(RigVertex, weights)};

    return attribs;
}