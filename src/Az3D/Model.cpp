#include "Az3D/Model.hpp"
#include "Az3D/Mesh.hpp"
#include "Az3D/Material.hpp"
#include "Az3D/MeshManager.hpp"
#include "Az3D/MaterialManager.hpp"
#include "Az3D/ResourceManager.hpp"

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

namespace Az3D {
    Model::Model(const std::string& meshId) : meshId(meshId) {
    }
    
    Model::Model(const std::string& meshId, const std::string& materialId) 
        : meshId(meshId), materialId(materialId) {
    }

    Mesh* Model::getMesh(const MeshManager& meshManager) const {
        if (meshId.empty()) return nullptr;
        // Now MeshManager uses indices, so Model needs a different approach
        // This method is deprecated - use ResourceManager instead
        return nullptr;
    }
    
    Material* Model::getMaterial(const MaterialManager& materialManager) const {
        if (materialId.empty()) return nullptr;
        // Now MaterialManager uses indices, so Model needs a different approach
        // This method is deprecated - use ResourceManager instead
        return nullptr;
    }

    // NEW: ResourceManager-based access (preferred method)
    Mesh* Model::getMesh(const ResourceManager& resourceManager) const {
        if (meshId.empty()) return nullptr;
        return resourceManager.getMesh(meshId);
    }
    
    Material* Model::getMaterial(const ResourceManager& resourceManager) const {
        if (materialId.empty()) return nullptr;
        return resourceManager.getMaterial(materialId);
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
        this->scalevec *= scaling;
    }

    void Model::scale(float uniformScale) {
        this->scalevec *= uniformScale;
    }

    glm::mat4 Model::getModelMatrix() const {
        glm::mat4 transform = glm::mat4(1.0f);

        transform = glm::translate(transform, position);
        transform = transform * glm::mat4_cast(rotation);
        transform = glm::scale(transform, scalevec);
        
        return transform;
    }

    void Model::resetTransform() {
        position = glm::vec3(0.0f);
        rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        scalevec = glm::vec3(1.0f);
    }

    void Model::updateTransform() {
        // Reserved for future use if needed
    }
}
