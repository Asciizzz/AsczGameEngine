#include "tinyCamera.hpp"
#include <algorithm>
#include <cmath>

tinyCamera::tinyCamera() 
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
    update();
}

tinyCamera::tinyCamera(const glm::vec3& position, float fov, float nearPlane, float farPlane)
    : pos(position)
    , orientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)) // No rotation
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
    // Convert quaternion to Euler angles at the start to avoid snapping issues
    glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(orientation));
    pitch = std::clamp(eulerAngles.x, -89.0f, 89.0f);
    yaw = eulerAngles.y;
    roll = eulerAngles.z;

    update();
}

void tinyCamera::setPos(const glm::vec3& newPos) {
    pos = newPos;
}

void tinyCamera::setRotation(float newPitch, float newYaw, float newRoll) {
    // Constrain pitch to avoid extreme values
    float constrainedPitch = std::clamp(newPitch, -89.0f, 89.0f);
    
    // Create quaternion from Euler angles (pitch, yaw, roll)
    glm::vec3 eulerRadians(glm::radians(constrainedPitch), glm::radians(newYaw), glm::radians(newRoll));
    orientation = glm::quat(eulerRadians);
    
    // Update cached Euler values
    pitch = constrainedPitch;
    yaw = newYaw;
    roll = newRoll;
}

void tinyCamera::setRotation(const glm::quat& quaternion) {
    orientation = glm::normalize(quaternion);
    
    // Update cached Euler values from quaternion
    glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(orientation));
    pitch = std::clamp(eulerAngles.x, -89.0f, 89.0f);
    yaw = eulerAngles.y;
    roll = eulerAngles.z;
}

void tinyCamera::setFOV(float newFov) {
    fov = std::clamp(newFov, 1.0f, 120.0f);
}

void tinyCamera::setNearFar(float newNearPlane, float newFarPlane) {
    nearPlane = newNearPlane;
    farPlane = newFarPlane;
}

void tinyCamera::setAspectRatio(float newAspectRatio) {
    aspectRatio = newAspectRatio;
}

void tinyCamera::translate(const glm::vec3& offset) {
    pos += offset;
}

void tinyCamera::rotate(float pitchDelta, float yawDelta, float rollDelta) {
    // Handle pitch and yaw in world space (traditional FPS style)
    if (pitchDelta != 0.0f || yawDelta != 0.0f) {
        // Update cached Euler angles for pitch/yaw
        pitch += pitchDelta;
        yaw += yawDelta;
        
        // Clamp pitch to prevent gimbal lock
        pitch = std::clamp(pitch, -89.0f, 89.0f);
        
        // Create base orientation from pitch/yaw only
        glm::vec3 eulerRadians(glm::radians(pitch), glm::radians(yaw), 0.0f);
        glm::quat baseOrientation = glm::quat(eulerRadians);
        
        // Apply existing roll on top of the base orientation
        glm::quat rollRotation = glm::angleAxis(glm::radians(roll), glm::vec3(0.0f, 0.0f, -1.0f));
        orientation = baseOrientation * rollRotation;
    }
    
    // Handle roll separately if provided
    if (rollDelta != 0.0f) {
        roll += rollDelta;

        // Rebuild orientation with new roll
        glm::vec3 eulerRadians(glm::radians(pitch), glm::radians(yaw), 0.0f);
        glm::quat baseOrientation = glm::quat(eulerRadians);
        glm::quat rollRotation = glm::angleAxis(glm::radians(roll), glm::vec3(0.0f, 0.0f, -1.0f));
        orientation = baseOrientation * rollRotation;
    }
    
    orientation = glm::normalize(orientation);
}

void tinyCamera::rotate(const glm::quat& deltaRotation) {
    orientation = orientation * deltaRotation;
    orientation = glm::normalize(orientation);
    
    // Update cached Euler values
    glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(orientation));
    pitch = std::clamp(eulerAngles.x, -89.0f, 89.0f);
    yaw = eulerAngles.y;
    roll = eulerAngles.z;
}

void tinyCamera::updateVectors() {
    // Use quaternion to rotate the standard basis vectors
    // Standard tinyCamera basis: forward = -Z, right = +X, up = +Y
    glm::vec3 baseForward = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 baseRight = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 baseUp = glm::vec3(0.0f, 1.0f, 0.0f);
    
    // Apply quaternion rotation to get current orientation vectors
    forward = glm::normalize(orientation * baseForward);
    right = glm::normalize(orientation * baseRight);
    up = glm::normalize(orientation * baseUp);
}

void tinyCamera::updateViewMatrix() {
    // Create view matrix using position and direction vectors
    viewMatrix = glm::lookAt(pos, pos + forward, up);
}

void tinyCamera::updateProjectionMatrix() {
    // Create perspective projection matrix
    // Note: GLM's perspective function expects FOV in radians
    projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    
    // Vulkan uses inverted Y axis compared to OpenGL
    projectionMatrix[1][1] *= -1;
}



static void extractFrustumPlanes(const glm::mat4& VP, tinyCamera::Plane planes[6]) {
    glm::vec4 r0 = glm::row(VP, 0);
    glm::vec4 r1 = glm::row(VP, 1);
    glm::vec4 r2 = glm::row(VP, 2);
    glm::vec4 r3 = glm::row(VP, 3);

    planes[0].eq = r3 + r0; // Left
    planes[1].eq = r3 - r0; // Right
    planes[2].eq = r3 + r1; // Bottom
    planes[3].eq = r3 - r1; // Top
    planes[4].eq = r3 + r2; // Near
    planes[5].eq = r3 - r2; // Far

    // Normalize planes
    for (int i = 0; i < 6; ++i) {
        glm::vec3 n = glm::vec3(planes[i].eq);
        float len = glm::length(n);
        planes[i].eq /= len;
    }
}

void tinyCamera::update() {
    updateVectors();
    updateViewMatrix();
    updateProjectionMatrix();

    extractFrustumPlanes(getVP(), planes);
}

// Quaternion-specific methods
void tinyCamera::setOrientation(const glm::quat& quat) {
    orientation = glm::normalize(quat);
    
    // Update cached Euler values
    glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(orientation));
    pitch = std::clamp(eulerAngles.x, -89.0f, 89.0f);
    yaw = eulerAngles.y;
    roll = eulerAngles.z;

    updateVectors();
    updateViewMatrix();
}

// Euler angle convenience functions
void tinyCamera::rotatePitch(float degrees) {
    rotate(degrees, 0.0f, 0.0f);
}

void tinyCamera::rotateYaw(float degrees) {
    rotate(0.0f, degrees, 0.0f);
}

void tinyCamera::rotateRoll(float degrees) {
    rotate(0.0f, 0.0f, degrees);
}

void tinyCamera::resetRoll() {
    // Reset roll to 0 while keeping pitch and yaw
    roll = 0.0f;
    
    // Rebuild orientation without roll
    glm::vec3 eulerRadians(glm::radians(pitch), glm::radians(yaw), 0.0f);
    orientation = glm::quat(eulerRadians);
    orientation = glm::normalize(orientation);
}

// Euler angle getters (computed from quaternion)
float tinyCamera::getPitch(bool radians) const {
    float pitchValue = glm::degrees(glm::eulerAngles(orientation)).x;
    pitchValue = std::clamp(pitchValue, -89.0f, 89.0f);

    return radians ? glm::radians(pitchValue) : pitchValue;
}

float tinyCamera::getYaw(bool radians) const {
    float yawValue = glm::degrees(glm::eulerAngles(orientation)).y;
    // Normalize angle to [-180, 180] range inline
    while (yawValue > 180.0f) yawValue -= 360.0f;
    while (yawValue < -180.0f) yawValue += 360.0f;
    return radians ? glm::radians(yawValue) : yawValue;
}

float tinyCamera::getRoll(bool radians) const {
    float rollValue = glm::degrees(glm::eulerAngles(orientation)).z;
    // Normalize angle to [-180, 180] range inline
    while (rollValue > 180.0f) rollValue -= 360.0f;
    while (rollValue < -180.0f) rollValue += 360.0f;

    return radians ? glm::radians(rollValue) : rollValue;
}


bool tinyCamera::collideAABB(glm::vec3 abMin, glm::vec3 abMax, glm::mat4 model) const {
    // Compute AABB center & half extents in local space
    glm::vec3 localCenter = (abMin + abMax) * 0.5f;
    glm::vec3 localHalf   = (abMax - abMin) * 0.5f;

    // Transform center to world
    glm::vec4 wc = model * glm::vec4(localCenter, 1.0f);
    glm::vec3 worldCenter = glm::vec3(wc);

    // Transform half extents by absolute value of 3x3 upper-left of model
    glm::mat3 rotScale = glm::mat3(model); // upper-left 3x3
    glm::mat3 absRS = glm::mat3(
        glm::abs(rotScale[0]),
        glm::abs(rotScale[1]),
        glm::abs(rotScale[2])
    );
    glm::vec3 worldHalf = absRS * localHalf;

    // Test against planes (plane eq is normalized)
    for (int i = 0; i < 6; ++i) {
        glm::vec3 n = glm::vec3(planes[i].eq);
        float d = planes[i].eq.w;
        float radius = worldHalf.x * fabs(n.x) + worldHalf.y * fabs(n.y) + worldHalf.z * fabs(n.z);
        float distance = glm::dot(n, worldCenter) + d;
        if (distance + radius < 0.0f) return false; // outside
    }
    return true;
}