#include "Az3D/Model.hpp"

namespace Az3D {
    Model::Model(std::shared_ptr<Mesh> mesh) : mesh(mesh) {
    }

    void Model::setMesh(std::shared_ptr<Mesh> mesh) {
        this->mesh = mesh;
    }

    void Model::setPosition(const glm::vec3& position) {
        this->position = position;
    }

    void Model::setRotation(const glm::vec3& rotation) {
        this->rotation = rotation;
    }

    void Model::setScale(const glm::vec3& scale) {
        this->scaleVec = scale;
    }

    void Model::setScale(float uniformScale) {
        this->scaleVec = glm::vec3(uniformScale);
    }

    void Model::translate(const glm::vec3& translation) {
        this->position += translation;
    }

    void Model::rotate(const glm::vec3& rotation) {
        this->rotation += rotation;
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
        
        // Apply rotations in order: Y (yaw) -> X (pitch) -> Z (roll)
        transform = glm::rotate(transform, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        transform = glm::rotate(transform, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        transform = glm::rotate(transform, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        
        transform = glm::scale(transform, scaleVec);
        
        return transform;
    }

    void Model::resetTransform() {
        position = glm::vec3(0.0f);
        rotation = glm::vec3(0.0f);
        scaleVec = glm::vec3(1.0f);
    }

    void Model::updateTransform() {
        // Reserved for future use if needed
    }
}
