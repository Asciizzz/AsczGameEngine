#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Az3D {

class Camera {
public:
    Camera();
    Camera(const glm::vec3& position, float fov = 45.0f, float nearPlane = 0.1f, float farPlane = 100.0f);
    ~Camera() = default;

    void setPosition(const glm::vec3& position);
    void setRotation(float pitch, float yaw, float roll = 0.0f);
    void setFOV(float fov);
    void setNearFar(float nearPlane, float farPlane);
    void setAspectRatio(float aspectRatio);

    void updateAspectRatio(uint32_t width, uint32_t height);
    void updateMatrices();

    void translate(const glm::vec3& offset);
    void rotate(float pitchDelta, float yawDelta, float rollDelta = 0.0f);

    void updateVectors();
    void updateViewMatrix();
    void updateProjectionMatrix();

    // Position and orientation
    glm::vec3 pos;
    float pitch;
    float yaw;
    float roll;

    // Projection parameters
    float fov;        // degrees
    float nearPlane;  
    float farPlane;   
    float aspectRatio; // width/height

    // Direction vectors
    glm::vec3 forward;  
    glm::vec3 up;       
    glm::vec3 right;    

    // Matrices
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;

    glm::mat4 getMVP() const { return projectionMatrix * viewMatrix; }
};

}
