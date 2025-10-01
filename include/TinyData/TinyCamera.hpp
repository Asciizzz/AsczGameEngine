#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

class TinyCamera {
public:
    TinyCamera();
    TinyCamera(const glm::vec3& position, float fov = 45.0f, float nearPlane = 0.1f, float farPlane = 100.0f);
    ~TinyCamera() = default;

    void setPosition(const glm::vec3& position);
    void setRotation(float pitch, float yaw, float roll = 0.0f);
    void setRotation(const glm::quat& quaternion);

    void setFOV(float fov);
    void setNearFar(float nearPlane, float farPlane);
    void setAspectRatio(float aspectRatio);

    void updateAspectRatio(uint32_t width, uint32_t height);
    void updateMatrices();

    void translate(const glm::vec3& offset);
    void rotate(float pitchDelta, float yawDelta, float rollDelta = 0.0f);
    void rotate(const glm::quat& deltaRotation);

    // Euler angle convenience functions
    void rotatePitch(float degrees);
    void rotateYaw(float degrees);
    void rotateRoll(float degrees);
    void resetRoll();
    
    // Quaternion access
    glm::quat getOrientation() const { return orientation; }
    void setOrientation(const glm::quat& quat);
    
    // Euler angle getters (computed from quaternion)
    float getPitch(bool radians = false) const;
    float getYaw(bool radians = false) const; 
    float getRoll(bool radians = false) const;

    void updateVectors();
    void updateViewMatrix();
    void updateProjectionMatrix();

    // Position and orientation
    glm::vec3 pos;
    glm::quat orientation;  // Primary orientation storage (quaternion)
    
    // Deprecated: Kept for backward compatibility (computed from quaternion)
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