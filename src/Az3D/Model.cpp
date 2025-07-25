#include "Az3D/Model.hpp"

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

namespace Az3D {

    Model::Model(size_t meshIndex, size_t materialIndex, bool hasBVH)
    : meshIndex(meshIndex), materialIndex(materialIndex), hasBVH(hasBVH) {}

    // Transformation

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

    void Transform::scale(const glm::vec3& scaling) {
        this->scl *= scaling;
    }

    void Transform::scale(float uniformScale) {
        this->scl *= uniformScale;
    }

    void Transform::rotate(const glm::vec3& eulerAngles) {
        // Convert Euler angles to quaternion and apply
        glm::quat eulerQuat = glm::quat(eulerAngles);
        this->rot = eulerQuat * this->rot;
    }


    glm::mat4 Transform::modelMatrix() const {
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

}
