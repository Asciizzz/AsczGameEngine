#pragma once

#include "Az3D/Mesh.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>

namespace Az3D {
    // Model class contains mesh and transformation
    class Model {
    public:
        Model() = default;
        Model(std::shared_ptr<Mesh> mesh);
        
        // Mesh management
        void setMesh(std::shared_ptr<Mesh> mesh);
        std::shared_ptr<Mesh> getMesh() const { return mesh; }
        
        // Transformation methods
        void setPosition(const glm::vec3& position);
        void setRotation(const glm::vec3& rotation); // Euler angles in radians
        void setScale(const glm::vec3& scale);
        void setScale(float uniformScale);
        
        void translate(const glm::vec3& translation);
        void rotate(const glm::vec3& rotation); // Euler angles in radians
        void scale(const glm::vec3& scaling);
        void scale(float uniformScale);
        
        // Transform getters
        const glm::vec3& getPosition() const { return position; }
        const glm::vec3& getRotation() const { return rotation; }
        const glm::vec3& getScale() const { return scaleVec; }
        
        // Matrix calculation
        glm::mat4 getModelMatrix() const;
        
        // Reset transform to identity
        void resetTransform();

    private:
        std::shared_ptr<Mesh> mesh;
        
        // Transform components
        glm::vec3 position{0.0f};
        glm::vec3 rotation{0.0f}; // Euler angles in radians
        glm::vec3 scaleVec{1.0f};
        
        // Helper to update internal state if needed
        void updateTransform();
    };
}
