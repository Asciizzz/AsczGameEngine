#include "Az3D/Model.hpp"
#include <glm/gtc/quaternion.hpp>

namespace Az3D {
    Model::Model(std::shared_ptr<Mesh> mesh) : mesh(mesh) {
    }

    void Model::setMesh(std::shared_ptr<Mesh> mesh) {
        this->mesh = mesh;
    }

    void Model::translate(const glm::vec3& translation) {
        this->position += translation;
    }

    void Model::rotate(const glm::quat& quaternion) {
        this->rotation = quaternion * this->rotation; // Multiply quaternion rotations
    }

    void Model::rotate(const glm::vec3& eulerAngles) {
        // Convert Euler angles to quaternion and apply
        glm::quat eulerQuat = glm::quat(eulerAngles);
        this->rotation = eulerQuat * this->rotation;
    }

    void Model::rotateX(float radians) {
        glm::quat xRotation = glm::angleAxis(radians, glm::vec3(1.0f, 0.0f, 0.0f));
        this->rotation = xRotation * this->rotation;
    }

    void Model::rotateY(float radians) {
        glm::quat yRotation = glm::angleAxis(radians, glm::vec3(0.0f, 1.0f, 0.0f));
        this->rotation = yRotation * this->rotation;
    }

    void Model::rotateZ(float radians) {
        glm::quat zRotation = glm::angleAxis(radians, glm::vec3(0.0f, 0.0f, 1.0f));
        this->rotation = zRotation * this->rotation;
    }

    void Model::scale(const glm::vec3& scaling) {
        this->scaleVec *= scaling;
    }

    void Model::scale(float uniformScale) {
        this->scaleVec *= uniformScale;
    }

    glm::mat4 Model::getModelMatrix() const {
        glm::mat4 transform = glm::mat4(1.0f);
        
        // Apply transformations in order: Scale -> Rotate -> Translate
        transform = glm::translate(transform, position);
        
        // Convert quaternion to rotation matrix and apply
        transform = transform * glm::mat4_cast(rotation);
        
        transform = glm::scale(transform, scaleVec);
        
        return transform;
    }

    void Model::resetTransform() {
        position = glm::vec3(0.0f);
        rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
        scaleVec = glm::vec3(1.0f);
    }

    void Model::updateTransform() {
        // Reserved for future use if needed
    }
}
