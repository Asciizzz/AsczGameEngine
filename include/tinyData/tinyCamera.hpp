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

    glm::mat4 getVP() const { return projectionMatrix * viewMatrix; }

    struct Plane {
        glm::vec4 eq; // Plane equation: ax + by + cz + d = 0
    };

    static void extractFrustumPlanes(const glm::mat4& VP, Plane planes[6]) {
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

    bool collideAABB(glm::vec3 abMin, glm::vec3 abMax, glm::mat4 model) const {
        // Extract planes once outside if calling per object many times.
        Plane planes[6];
        extractFrustumPlanes(getVP(), planes); // ensure these are normalized inside

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

};