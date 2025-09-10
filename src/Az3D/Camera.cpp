#include "Az3D/Camera.hpp"
#include <algorithm>

using namespace Az3D;

Camera::Camera() 
    : pos(0.0f, 0.0f, 0.0f)
    , orientation(glm::quat(glm::vec3(0.0f, glm::radians(-90.0f), 0.0f))) // Start looking down negative Z axis
    , pitch(0.0f)
    , yaw(-90.0f) 
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
    : pos(position)
    , orientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)) // No rotation
    , pitch(0.0f)
    , yaw(-90.0f)  
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

void Camera::setPosition(const glm::vec3& newPos) {
    pos = newPos;
    updateViewMatrix();
}

void Camera::setRotation(float newPitch, float newYaw, float newRoll) {
    // Constrain pitch to avoid extreme values
    float constrainedPitch = std::clamp(newPitch, -89.0f, 89.0f);
    
    // Create quaternion from Euler angles (pitch, yaw, roll)
    glm::vec3 eulerRadians(glm::radians(constrainedPitch), glm::radians(newYaw), glm::radians(newRoll));
    orientation = glm::quat(eulerRadians);
    
    // Update cached Euler values
    pitch = constrainedPitch;
    yaw = newYaw;
    roll = newRoll;

    updateVectors();
    updateViewMatrix();
}

void Camera::setRotation(const glm::quat& quaternion) {
    orientation = glm::normalize(quaternion);
    
    // Update cached Euler values from quaternion
    glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(orientation));
    pitch = eulerAngles.x;
    yaw = eulerAngles.y;
    roll = eulerAngles.z;

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
    pos += offset;
    updateViewMatrix();
}

void Camera::rotate(float pitchDelta, float yawDelta, float rollDelta) {
    // Update cached Euler angles first (for pitch clamping)
    pitch += pitchDelta;
    yaw += yawDelta; 
    roll += rollDelta;
    
    // Clamp pitch to prevent gimbal lock
    pitch = std::clamp(pitch, -89.0f, 89.0f);
    
    // Create quaternion from the updated Euler angles
    glm::vec3 eulerRadians(glm::radians(pitch), glm::radians(yaw), glm::radians(roll));
    orientation = glm::quat(eulerRadians);
    orientation = glm::normalize(orientation);

    updateVectors();
    updateViewMatrix();
}

void Camera::rotate(const glm::quat& deltaRotation) {
    orientation = orientation * deltaRotation;
    orientation = glm::normalize(orientation);
    
    // Update cached Euler values
    glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(orientation));
    pitch = std::clamp(eulerAngles.x, -89.0f, 89.0f);
    yaw = eulerAngles.y;
    roll = eulerAngles.z;

    updateVectors();
    updateViewMatrix();
}

void Camera::updateVectors() {
    // Use quaternion to rotate the standard basis vectors
    // Standard camera basis: forward = -Z, right = +X, up = +Y
    glm::vec3 baseForward = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 baseRight = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 baseUp = glm::vec3(0.0f, 1.0f, 0.0f);
    
    // Apply quaternion rotation to get current orientation vectors
    forward = glm::normalize(orientation * baseForward);
    right = glm::normalize(orientation * baseRight);
    up = glm::normalize(orientation * baseUp);
}

void Camera::updateViewMatrix() {
    // Create view matrix using position and direction vectors
    viewMatrix = glm::lookAt(pos, pos + forward, up);
}

void Camera::updateProjectionMatrix() {
    // Create perspective projection matrix
    // Note: GLM's perspective function expects FOV in radians
    projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    
    // Vulkan uses inverted Y axis compared to OpenGL
    projectionMatrix[1][1] *= -1;
}

// Quaternion-specific methods
void Camera::setOrientation(const glm::quat& quat) {
    orientation = glm::normalize(quat);
    
    // Update cached Euler values
    glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(orientation));
    pitch = eulerAngles.x;
    yaw = eulerAngles.y;
    roll = eulerAngles.z;
    
    updateVectors();
    updateViewMatrix();
}

// Euler angle convenience functions
void Camera::rotatePitch(float degrees) {
    rotate(degrees, 0.0f, 0.0f);
}

void Camera::rotateYaw(float degrees) {
    rotate(0.0f, degrees, 0.0f);
}

void Camera::rotateRoll(float degrees) {
    rotate(0.0f, 0.0f, degrees);
}

// Euler angle getters (computed from quaternion)
float Camera::getPitch() const {
    return glm::degrees(glm::eulerAngles(orientation)).x;
}

float Camera::getYaw() const {
    return glm::degrees(glm::eulerAngles(orientation)).y;
}

float Camera::getRoll() const {
    return glm::degrees(glm::eulerAngles(orientation)).z;
}
