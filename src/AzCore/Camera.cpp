#include "AzCore/Camera.hpp"
#include <algorithm>

namespace AzCore {
    Camera::Camera() 
        : position(0.0f, 0.0f, 3.0f)
        , pitch(0.0f)
        , yaw(-90.0f)  // Start looking down negative Z axis
        , roll(0.0f)
        , fov(45.0f)
        , nearPlane(0.1f)
        , farPlane(100.0f)
        , aspectRatio(16.0f / 9.0f)
        , forward(0.0f, 0.0f, -1.0f)
        , up(0.0f, 1.0f, 0.0f)
        , right(1.0f, 0.0f, 0.0f)
        , viewMatrix(1.0f)
        , projectionMatrix(1.0f)
    {
        updateVectors();
        updateMatrices();
    }

    Camera::Camera(const glm::vec3& position, float fov, float nearPlane, float farPlane)
        : position(position)
        , pitch(0.0f)
        , yaw(-90.0f)  // Start looking down negative Z axis
        , roll(0.0f)
        , fov(fov)
        , nearPlane(nearPlane)
        , farPlane(farPlane)
        , aspectRatio(16.0f / 9.0f)
        , forward(0.0f, 0.0f, -1.0f)
        , up(0.0f, 1.0f, 0.0f)
        , right(1.0f, 0.0f, 0.0f)
        , viewMatrix(1.0f)
        , projectionMatrix(1.0f)
    {
        updateVectors();
        updateMatrices();
    }

    void Camera::setPosition(const glm::vec3& newPosition) {
        position = newPosition;
        updateViewMatrix();
    }

    void Camera::setRotation(float newPitch, float newYaw, float newRoll) {
        pitch = newPitch;
        yaw = newYaw;
        roll = newRoll;

        // Constrain pitch to avoid gimbal lock
        pitch = std::clamp(pitch, -89.0f, 89.0f);

        updateVectors();
        updateViewMatrix();
    }

    void Camera::setFOV(float newFov) {
        fov = std::clamp(newFov, 1.0f, 120.0f);
        updateProjectionMatrix();
    }

    void Camera::setNearFar(float newNearPlane, float newFarPlane) {
        nearPlane = newNearPlane;
        farPlane = newFarPlane;
        updateProjectionMatrix();
    }

    void Camera::setAspectRatio(float newAspectRatio) {
        aspectRatio = newAspectRatio;
        updateProjectionMatrix();
    }

    void Camera::updateAspectRatio(uint32_t width, uint32_t height) {
        if (height > 0) {
            aspectRatio = static_cast<float>(width) / static_cast<float>(height);
            updateProjectionMatrix();
        }
    }

    void Camera::updateMatrices() {
        updateVectors();
        updateViewMatrix();
        updateProjectionMatrix();
    }

    void Camera::translate(const glm::vec3& offset) {
        position += offset;
        updateViewMatrix();
    }

    void Camera::rotate(float pitchDelta, float yawDelta, float rollDelta) {
        pitch += pitchDelta;
        yaw += yawDelta;
        roll += rollDelta;

        // Constrain pitch to avoid gimbal lock
        pitch = std::clamp(pitch, -89.0f, 89.0f);

        updateVectors();
        updateViewMatrix();
    }

    void Camera::updateVectors() {
        // Convert angles to radians
        float pitchRad = glm::radians(pitch);
        float yawRad = glm::radians(yaw);
        float rollRad = glm::radians(roll);

        // Calculate forward vector from pitch and yaw
        glm::vec3 newForward;
        newForward.x = cos(yawRad) * cos(pitchRad);
        newForward.y = sin(pitchRad);
        newForward.z = sin(yawRad) * cos(pitchRad);
        forward = glm::normalize(newForward);

        // Calculate right vector (perpendicular to forward and world up)
        glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
        right = glm::normalize(glm::cross(forward, worldUp));

        // Calculate up vector (perpendicular to forward and right)
        glm::vec3 localUp = glm::normalize(glm::cross(right, forward));

        // Apply roll rotation to the up vector
        if (abs(rollRad) > 0.001f) {
            // Create rotation matrix around forward axis (roll)
            glm::mat4 rollMatrix = glm::rotate(glm::mat4(1.0f), rollRad, forward);
            up = glm::normalize(glm::vec3(rollMatrix * glm::vec4(localUp, 0.0f)));
            
            // Recalculate right vector to maintain orthogonality
            right = glm::normalize(glm::cross(forward, up));
        } else {
            up = localUp;
        }
    }

    void Camera::updateViewMatrix() {
        // Create view matrix using position and direction vectors
        viewMatrix = glm::lookAt(position, position + forward, up);
    }

    void Camera::updateProjectionMatrix() {
        // Create perspective projection matrix
        // Note: GLM's perspective function expects FOV in radians
        projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
        
        // Vulkan uses inverted Y axis compared to OpenGL
        projectionMatrix[1][1] *= -1;
    }
}
