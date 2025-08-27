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
    glm::mat4 rotMat = glm::mat4_cast(rot);
    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scl);

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
    pos_tu.w = uv.x;
    nrml_tv.w = uv.y;
}

VkVertexInputBindingDescription VertexStatic::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(VertexStatic);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 2> VertexStatic::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

    // Position and texture u
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(VertexStatic, pos_tu);

    // Normal and texture v
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(VertexStatic, nrml_tv);

    return attributeDescriptions;
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
    pos_tu.w = uv.x;
    nrml_tv.w = uv.y;
}

VkVertexInputBindingDescription VertexSkinned::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(VertexSkinned);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 4> VertexSkinned::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

    // Position and texture u
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(VertexSkinned, pos_tu);

    // Normal and texture v
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(VertexSkinned, nrml_tv);

    // Bone IDs
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SINT;
    attributeDescriptions[2].offset = offsetof(VertexSkinned, boneIDs);

    // Weights
    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(VertexSkinned, weights);

    return attributeDescriptions;
}

} // namespace Az3D