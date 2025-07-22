#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace AzCore {
    class Camera {
    public:
        Camera();
        Camera(const glm::vec3& position, float fov = 45.0f, float nearPlane = 0.1f, float farPlane = 100.0f);
        ~Camera() = default;

        // Position and orientation
        void setPosition(const glm::vec3& position);
        void setRotation(float pitch, float yaw, float roll = 0.0f);
        void setFOV(float fov);
        void setNearFar(float nearPlane, float farPlane);
        void setAspectRatio(float aspectRatio);

        // Update methods
        void updateAspectRatio(uint32_t width, uint32_t height);
        void updateMatrices();

        // Movement helpers (for future input implementation)
        void translate(const glm::vec3& offset);
        void rotate(float pitchDelta, float yawDelta, float rollDelta = 0.0f);

        // Internal update methods (now public for direct access)
        void updateVectors();
        void updateViewMatrix();
        void updateProjectionMatrix();

        // All data members are now public for direct access
        // Position and orientation
        glm::vec3 position;
        float pitch;  // X-axis rotation (up/down)
        float yaw;    // Y-axis rotation (left/right)
        float roll;   // Z-axis rotation (tilt)

        // Projection parameters
        float fov;        // Field of view in degrees
        float nearPlane;  // Near clipping plane
        float farPlane;   // Far clipping plane
        float aspectRatio; // Width/Height ratio

        // Direction vectors
        glm::vec3 forward;  // Camera's forward direction
        glm::vec3 up;       // Camera's up direction
        glm::vec3 right;    // Camera's right direction

        // Matrices
        glm::mat4 viewMatrix;
        glm::mat4 projectionMatrix;

        // Convenience methods for common access patterns
        glm::mat4 getViewProjectionMatrix() const { return projectionMatrix * viewMatrix; }
    };
}
