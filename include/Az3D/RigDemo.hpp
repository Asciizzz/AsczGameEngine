#pragma once

#include "Az3D/TinyModel.hpp"
#include "AzVulk/Buffer.hpp"
#include "AzVulk/Descriptor.hpp"

#include <iostream>

namespace Az3D {

// Demo/testing struct that does fun things with the skeleton
struct RigDemo {
    SharedPtr<TinySkeleton> skeleton;
    glm::mat4 getBindPose(size_t index);

    std::vector<glm::mat4> localPoseTransforms; // <- User changeable

    std::vector<glm::mat4> globalPoseTransforms; // <--DO NOT CHANGE-- Recursive transforms

    std::vector<glm::mat4> finalTransforms; // <--DO NOT CHANGE-- GPU eat this up!

    AzVulk::BufferData finalPoseBuffer;
    AzVulk::DescLayout descLayout;
    AzVulk::DescPool   descPool;
    AzVulk::DescSets   descSet;

    size_t meshIndex = 0; // Which mesh to apply this skeleton to

    void computeBone(const std::string& boneName);
    void computeBone(size_t boneIndex);

    void computeAllTransforms();
    void computeBoneRecursive(size_t boneIndex, std::vector<bool>& processed);

    void init(const AzVulk::Device* vkDevice, const SharedPtr<TinySkeleton>& skeleton);
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
