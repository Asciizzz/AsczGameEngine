#include "Az3D/Model.hpp"

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

namespace Az3D {

    Model::Model(size_t meshIndex, size_t materialIndex)
        : meshIndex(meshIndex), materialIndex(materialIndex) {}

    // Transformation

    void Transform::translate(const glm::vec3& translation) {
        this->position += translation;
    }

    void Transform::rotate(const glm::quat& quaternion) {
        this->rotation = quaternion * this->rotation; // Multiply quaternion rotations
    }

    void Transform::rotateX(float radians) {
        glm::quat xRotation = glm::angleAxis(radians, glm::vec3(1.0f, 0.0f, 0.0f));
        this->rotation = xRotation * this->rotation;
    }

    void Transform::rotateY(float radians) {
        glm::quat yRotation = glm::angleAxis(radians, glm::vec3(0.0f, 1.0f, 0.0f));
        this->rotation = yRotation * this->rotation;
    }

    void Transform::rotateZ(float radians) {
        glm::quat zRotation = glm::angleAxis(radians, glm::vec3(0.0f, 0.0f, 1.0f));
        this->rotation = zRotation * this->rotation;
    }

    void Transform::scale(const glm::vec3& scaling) {
        this->scalevec *= scaling;
    }

    void Transform::scale(float uniformScale) {
        this->scalevec *= uniformScale;
    }

    void Transform::rotate(const glm::vec3& eulerAngles) {
        // Convert Euler angles to quaternion and apply
        glm::quat eulerQuat = glm::quat(eulerAngles);
        this->rotation = eulerQuat * this->rotation;
    }


    glm::mat4 Transform::modelMatrix() const {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 rotationMat = glm::mat4_cast(rotation);
        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scalevec);

        return translation * rotationMat * scaleMat;
    }


    void Transform::reset() {
        position = glm::vec3(0.0f);
        rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        scalevec = glm::vec3(1.0f);
    }

}
