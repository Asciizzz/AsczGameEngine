#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace Az3D {

    struct Transform {
        glm::vec3 pos{0.0f};
        glm::quat rot{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scl{1.0f};

        void translate(const glm::vec3& translation);
        void rotate(const glm::quat& rotation);
        void rotateX(float radians);
        void rotateY(float radians);  
        void rotateZ(float radians);
        void scale(const glm::vec3& scaling);
        void scale(float uniformScale);

        // Legacy methods for compatibility
        void rotate(const glm::vec3& eulerAngles);

        // Matrix calculation
        glm::mat4 modelMatrix() const;

        // Reset transform to identity
        void reset();
    };

    // Model class contains mesh ID, material ID, and transformation
    class Model {
    public:
        Model() = default;
        Model(size_t meshIndex, size_t materialIndex);

        size_t meshIndex = 0;
        size_t materialIndex = 0;

        // Transform component
        Transform trform;

        bool hasBVH = false; // Flag for BVH usage
    };
}
