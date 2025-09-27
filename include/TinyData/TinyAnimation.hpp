#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <string>

#include "Helpers/Templates.hpp"

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
    enum class TargetPath {
        Translation,
        Rotation,
        Scale,
        Weights  // Morph targets
    } targetPath = TargetPath::Translation;
    TinyAnimationChannel& setTargetPath(const std::string& pathStr);

    enum class TargetType {
        Bone,
        Node,
        Morph
    } targetType = TargetType::Bone;

    int targetIndex = -1;   // Node/bone/morph index in the node/resource system
    int samplerIndex = -1;  // Reference into TinyAnimation.samplers
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