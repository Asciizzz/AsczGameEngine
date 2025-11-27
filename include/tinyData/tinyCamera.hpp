#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/quaternion.hpp>

class tinyCamera {
public:
    tinyCamera();
    tinyCamera(const glm::vec3& position, float fov = 45.0f, float nearPlane = 0.1f, float farPlane = 100.0f);
    ~tinyCamera() noexcept = default;

    void setPos(const glm::vec3& position);
    void setRotation(float pitch, float yaw, float roll = 0.0f);
    void setRotation(const glm::quat& quaternion);

    void setFOV(float fov);
    void setNearFar(float nearPlane, float farPlane);
    void setAspectRatio(float aspectRatio);

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

    void update();

    glm::vec3 pos;
    glm::quat orientation;  // Primary orientation storage (quaternion)

    float pitch;
    float yaw;
    float roll;

    float fov;        // degrees
    float nearPlane;  
    float farPlane;   
    float aspectRatio; // width/height

    glm::vec3 forward;  
    glm::vec3 up;       
    glm::vec3 right;    

    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
    
    struct Plane {
        glm::vec4 eq; // Plane equation: ax + by + cz + d = 0
    };
    Plane planes[6];

    glm::mat4 getVP() const { return projectionMatrix * viewMatrix; }

    bool collideAABB(const glm::vec3& abMin, const glm::vec3& abMax, const glm::mat4& model) const;

};