#pragma once

#include "TinyModel.hpp"
#include "TinyPlayback.hpp"

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
    SharedPtr<TinyModel> model;
    TinyPlayback playback;

    AzVulk::BufferData finalPoseBuffer;
    AzVulk::DescLayout descLayout;
    AzVulk::DescPool   descPool;
    AzVulk::DescSets   descSet;

    size_t modelIndex = 0;


    void init(const AzVulk::Device* vkDevice, const TinyModel& model, size_t modelIndex);
    void update(float dTime);
    void updateBuffer();
    void updatePlayback(float dTime);

    void playAnimation(size_t animIndex=0, bool loop=true, float speed=1.0f, float transitionTime=0.3f);
};

} // namespace Az3D
