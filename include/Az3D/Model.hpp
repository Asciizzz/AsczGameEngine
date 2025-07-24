#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace Az3D {
    // Forward declarations
    class Mesh;
    class Material;
    class MeshManager;
    class MaterialManager;
    
    // Model class contains mesh ID, material ID, and transformation
    class Model {
    public:
        Model() = default;
        Model(const std::string& meshId);
        Model(const std::string& meshId, const std::string& materialId);
        
        // Mesh management (ID-based)
        void setMeshId(const std::string& meshId) { this->meshId = meshId; }
        const std::string& getMeshId() const { return meshId; }
        bool hasMesh() const { return !meshId.empty(); }
        
        // Material management (ID-based)
        void setMaterialId(const std::string& materialId) { this->materialId = materialId; }
        const std::string& getMaterialId() const { return materialId; }
        bool hasMaterial() const { return !materialId.empty(); }
        
        // Resource access (requires managers)
        Mesh* getMesh(const MeshManager& meshManager) const;
        Material* getMaterial(const MaterialManager& materialManager) const;
        
        // Transform components - direct access for performance
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scalevec{1.0f};
        
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
        std::string meshId;
        std::string materialId;
        
        // Helper to update internal state if needed
        void updateTransform();
    };
}
