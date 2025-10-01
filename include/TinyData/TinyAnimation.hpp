#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <string>

#include ".ext/Templates.hpp"

struct TinyAnimationSampler {
    std::vector<float> inputTimes;        // keyframe times
    std::vector<glm::vec4> outputValues;  // generic container
                                          // vec3 for translation/scale, vec4 for rotation, vecN for weights
    enum class InterpolationType {
        Linear,
        Step,
        CubicSpline
    } interpolation = InterpolationType::Linear;

    TinyAnimationSampler& setInterpolation(const std::string& interpStr);
    TinyAnimationSampler& setInterpolation(const InterpolationType interpType);
};

struct TinyAnimationChannel {
    uint32_t nodeIndex = 0;
    uint32_t samplerIndex = 0;    // link to sampler

    enum class TargetPath {
        Translation,
        Rotation,
        Scale,
        Weights
    } targetPath;
    TinyAnimationChannel& setTargetPath(const std::string& pathStr);
    TinyAnimationChannel& setTargetPath(const TargetPath path);

    enum class TargetType {
        Node,
        Bone, 
        Morph
    } targetType = TargetType::Node;
    uint32_t targetIndex = 0;
};

struct TinyAnimation {
    std::string name;
    
    std::vector<TinyAnimationSampler> samplers;
    std::vector<TinyAnimationChannel> channels;
    float duration = 0.0f;  // Computed from all samplers

    void clear();

    // Helper methods
    void computeDuration();
    int findChannelForBone(int boneIndex, TinyAnimationChannel::TargetPath path) const;
};