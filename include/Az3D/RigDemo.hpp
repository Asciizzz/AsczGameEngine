#pragma once

#include "Az3D/TinyModel.hpp"
#include "AzVulk/Buffer.hpp"
#include "AzVulk/Descriptor.hpp"

#include <iostream>

namespace Az3D {

struct FunTransform {
    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    
    FunTransform() = default;
    FunTransform(const glm::vec3& t, const glm::quat& r, const glm::vec3& s) 
        : translation(t), rotation(r), scale(s) {}
};

struct BonePhysicsState {
    FunTransform localTransform; // current local transform (we will update rotation only)
    glm::vec3 angVel{0.0f};      // angular velocity in local bone space (rad/s)

    // Tweakable params (can be per-bone)
    float stiffness = 50.0f;  // spring constant: Nm/rad
    float damping   = 8.0f;   // viscous damping: Nms/rad
    float inertia   = 1.0f;   // scalar approx of moment of inertia
    float maxAngVel = 30.0f;  // clamp (rad/s)
    bool  useLimit  = false;
    float limitAngleDegrees = 180.0f;
};

// Demo/testing struct that does fun things with the skeleton
struct RigDemo {
    TinySkeleton skeleton;
    glm::mat4 getBindPose(size_t index);

    std::vector<glm::mat4> localPoseTransforms; // <- User changeable

    std::vector<glm::mat4> globalPoseTransforms; // <--DO NOT CHANGE-- Recursive transforms

    std::vector<glm::mat4> finalTransforms; // <--DO NOT CHANGE-- GPU eat this up!

    std::vector<BonePhysicsState> states;

    AzVulk::BufferData finalPoseBuffer;
    AzVulk::DescLayout descLayout;
    AzVulk::DescPool   descPool;
    AzVulk::DescSets   descSet;

    size_t modelIndex = 0; // Which model to apply this skeleton to

    void computeBone(const std::string& boneName);
    void computeBone(size_t boneIndex);

    void computeAllTransforms();
    void computeBoneRecursive(size_t boneIndex, std::vector<bool>& processed);

    void init(const AzVulk::Device* vkDevice, const TinySkeleton& skeleton, size_t modelIndex);
    void updateBuffer();

    float funAccumTimeValue = 0.0f;

    struct FunParams {
        std::vector<float> customFloat;
        std::vector<glm::vec2> customVec2;
        std::vector<glm::vec3> customVec3;
        std::vector<glm::vec4> customVec4;
        std::vector<glm::mat4> customMat4;

        size_t add(float f) {
            customFloat.push_back(f);
            return customFloat.size() - 1;
        }

        size_t add(const glm::vec2& v) {
            customVec2.push_back(v);
            return customVec2.size() - 1;
        }

        size_t add(const glm::vec3& v) {
            customVec3.push_back(v);
            return customVec3.size() - 1;
        }

        size_t add(const glm::vec4& v) {
            customVec4.push_back(v);
            return customVec4.size() - 1;
        }

        size_t add(const glm::mat4& m) {
            customMat4.push_back(m);
            return customMat4.size() - 1;
        }
    };

    void funFunction(const FunParams& params);

};

} // namespace Az3D
