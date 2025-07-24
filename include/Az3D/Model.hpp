#pragma once

#include "Az3D/Mesh.hpp"
#include "Az3D/Material.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>

namespace Az3D {
    // Model class contains mesh, material, and transformation
    class Model {
    public:
        Model() = default;
        Model(std::shared_ptr<Mesh> mesh);
        Model(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material);
        
        // Mesh management
        void setMesh(std::shared_ptr<Mesh> mesh);
        std::shared_ptr<Mesh> getMesh() const { return mesh; }
        
        // Material management
        void setMaterial(std::shared_ptr<Material> material);
        std::shared_ptr<Material> getMaterial() const { return material; }
        bool hasMaterial() const { return material != nullptr; }
        
        // Transform components - direct access for performance
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scaleVec{1.0f};
        
        // Convenience transformation methods (with additional logic)
        void translate(const glm::vec3& translation);
        void rotate(const glm::quat& rotation); // Quaternion rotation
        void rotateX(float radians);
        void rotateY(float radians);  
        void rotateZ(float radians);
        void scale(const glm::vec3& scaling);
        void scale(float uniformScale);

        // Legacy methods for compatibility
        void rotate(const glm::vec3& eulerAngles); // Euler angles (not recommended because of gimbal lock)
        
        // Matrix calculation
        glm::mat4 getModelMatrix() const;
        
        // Reset transform to identity
        void resetTransform();

    private:
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<Material> material;
        
        // Helper to update internal state if needed
        void updateTransform();
    };
}
