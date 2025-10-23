#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <string>

#include ".ext/Templates.hpp"

struct TinyAnimeSampler {
    std::vector<float> inputTimes;        // keyframe times
    std::vector<glm::vec4> outputValues;  // generic container
                                          // vec3 for translation/scale, vec4 for rotation, vecN for weights
    enum class InterpolationType {
        Linear,
        Step,
        CubicSpline
    } interpolation = InterpolationType::Linear;

    TinyAnimeSampler& setInterpolation(const std::string& interpStr);
    TinyAnimeSampler& setInterpolation(const InterpolationType interpType);
};

struct TinyAnimeChannel {
    uint32_t nodeIndex = 0;
    uint32_t samplerIndex = 0;    // link to sampler

    enum class TargetPath {
        Translation,
        Rotation,
        Scale,
        Weights
    } targetPath;
    TinyAnimeChannel& setTargetPath(const std::string& pathStr);
    TinyAnimeChannel& setTargetPath(const TargetPath path);

    enum class TargetType {
        Node,
        Bone, 
        Morph
    } targetType = TargetType::Node;
    uint32_t targetIndex = 0;
};

struct TinyAnime {
    std::string name;
    
    std::vector<TinyAnimeSampler> samplers;
    std::vector<TinyAnimeChannel> channels;
    float duration = 0.0f;  // Computed from all samplers

    void clear();

    // Helper methods
    void computeDuration();
    int findChannelForBone(int boneIndex, TinyAnimeChannel::TargetPath path) const;
};

// Above are obsolete legacy structures kept for reference.


struct TinyAnimeRT {
    // Place holder
};