#include "Az3D/TinyAnimation.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>

using namespace Az3D;

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
        if (channels[i].targetBoneIndex == boneIndex && channels[i].targetPath == path) {
            return i;
        }
    }
    return -1; // Not found
}