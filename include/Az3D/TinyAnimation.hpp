#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <vector>
#include <string>

#include "Helpers/Templates.hpp"

namespace Az3D {

struct TinyAnimationSampler {
    std::vector<float> inputTimes;        // Time values (in seconds)
    std::vector<glm::vec3> translations;  // Translation values (if target_path is "translation")
    std::vector<glm::quat> rotations;     // Rotation values (if target_path is "rotation") 
    std::vector<glm::vec3> scales;        // Scale values (if target_path is "scale")
    std::vector<float> weights;           // Morph target weights (if target_path is "weights")
    
    enum class InterpolationType {
        Linear,
        Step,
        CubicSpline
    };
    
    InterpolationType interpolation = InterpolationType::Linear;
    
    // Helper methods to determine what type of data this sampler contains
    bool hasTranslations() const { return !translations.empty(); }
    bool hasRotations() const { return !rotations.empty(); }
    bool hasScales() const { return !scales.empty(); }
    bool hasWeights() const { return !weights.empty(); }
};

struct TinyAnimationChannel {
    int samplerIndex = -1;        // Index into TinyAnimation::samplers
    int targetBoneIndex = -1;     // Index into TinySkeleton bone arrays (-1 if targets the whole model)
    
    enum class TargetPath {
        Translation,
        Rotation, 
        Scale,
        Weights  // For morph targets
    };
    
    TargetPath targetPath = TargetPath::Translation;
};

struct TinyAnimation {
    std::string name;
    
    std::vector<TinyAnimationSampler> samplers;
    std::vector<TinyAnimationChannel> channels;
    
    float duration = 0.0f;  // Computed from all samplers
    
    // Helper methods
    void computeDuration();
    int findChannelForBone(int boneIndex, TinyAnimationChannel::TargetPath path) const;
    void printDebug(const std::vector<std::string>& boneNames = {}) const;
};

}