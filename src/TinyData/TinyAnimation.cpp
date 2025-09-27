#include "TinyData/TinyAnimation.hpp"

#include <iostream>
#include <iomanip>
#include <algorithm>

TinyAnimationSampler& TinyAnimationSampler::setInterpolation(const std::string& interpStr) {
    setInterpolation(InterpolationType::Linear);

    if (interpStr == "STEP") setInterpolation(InterpolationType::Step); else
    if (interpStr == "CUBICSPLINE") setInterpolation(InterpolationType::CubicSpline);

    return *this;
}

TinyAnimationSampler& TinyAnimationSampler::setInterpolation(const InterpolationType interpType) {
    interpolation = interpType;
    return *this;
}

TinyAnimationChannel& TinyAnimationChannel::setTargetPath(const std::string& pathStr) {
    if (pathStr == "translation") targetPath = TargetPath::Translation; else
    if (pathStr == "rotation")    targetPath = TargetPath::Rotation;    else
    if (pathStr == "scale")       targetPath = TargetPath::Scale;       else
    if (pathStr == "weights")     targetPath = TargetPath::Weights;     else
                                  targetPath = TargetPath::Translation;
    return *this;
}


void TinyAnimation::clear() {
    name.clear();
    samplers.clear();
    channels.clear();
    duration = 0.0f;
}

void TinyAnimation::computeDuration() {
    duration = 0.0f;
    
    for (const auto& sampler : samplers) {
        if (!sampler.inputTimes.empty()) {
            float samplerDuration = *std::max_element(sampler.inputTimes.begin(), sampler.inputTimes.end());
            duration = std::max(duration, samplerDuration);
        }
    }
}

int TinyAnimation::findChannelForBone(int boneIndex, TinyAnimationChannel::TargetPath path) const {
    for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
        if (channels[i].targetIndex == boneIndex && channels[i].targetPath == path) {
            return i;
        }
    }
    return -1; // Not found
}