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

void TinyAnimation::printDebug(const std::vector<std::string>& boneNames) const {
    std::cout << "Animation: \"" << name << "\" (Duration: " << std::fixed << std::setprecision(2) << duration << "s)\n";
    std::cout << std::string(60, '=') << "\n";
    
    // Print samplers
    std::cout << "+-- Samplers (" << samplers.size() << " data containers)\n";
    for (size_t i = 0; i < samplers.size(); ++i) {
        const auto& sampler = samplers[i];
        std::string prefix = (i == samplers.size() - 1) ? "|   +-- " : "|   +-- ";
        std::cout << prefix << "Sampler[" << i << "] ";
        
        // Determine interpolation type
        std::string interpType;
        switch (sampler.interpolation) {
            case TinyAnimationSampler::InterpolationType::Linear:
                interpType = "LINEAR";
                break;
            case TinyAnimationSampler::InterpolationType::Step:
                interpType = "STEP";
                break;
            case TinyAnimationSampler::InterpolationType::CubicSpline:
                interpType = "CUBICSPLINE";
                break;
        }
        std::cout << "(" << interpType << ")\n";
        
        // Print input times
        std::string timePrefix = (i == samplers.size() - 1) ? "|       " : "|   |   ";
        std::cout << timePrefix << "+-- Input times: [";
        for (size_t j = 0; j < std::min(sampler.inputTimes.size(), size_t(5)); ++j) {
            if (j > 0) std::cout << ", ";
            std::cout << std::fixed << std::setprecision(2) << sampler.inputTimes[j];
        }
        if (sampler.inputTimes.size() > 5) {
            std::cout << ", ... (" << sampler.inputTimes.size() << " total)";
        }
        std::cout << "]\n";
        
        // Print output values based on what's available
        std::cout << timePrefix << "+-- Output values: ";
        if (sampler.hasTranslations()) {
            std::cout << "[";
            for (size_t j = 0; j < std::min(sampler.translations.size(), size_t(3)); ++j) {
                if (j > 0) std::cout << ", ";
                const auto& t = sampler.translations[j];
                std::cout << "vec3(" << std::fixed << std::setprecision(1) 
                         << t.x << "," << t.y << "," << t.z << ")";
            }
            if (sampler.translations.size() > 3) {
                std::cout << ", ... (" << sampler.translations.size() << " translations)";
            }
            std::cout << "]\n";
        } else if (sampler.hasRotations()) {
            std::cout << "[";
            for (size_t j = 0; j < std::min(sampler.rotations.size(), size_t(3)); ++j) {
                if (j > 0) std::cout << ", ";
                const auto& r = sampler.rotations[j];
                std::cout << "quat(" << std::fixed << std::setprecision(2) 
                         << r.w << "," << r.x << "," << r.y << "," << r.z << ")";
            }
            if (sampler.rotations.size() > 3) {
                std::cout << ", ... (" << sampler.rotations.size() << " rotations)";
            }
            std::cout << "]\n";
        } else if (sampler.hasScales()) {
            std::cout << "[";
            for (size_t j = 0; j < std::min(sampler.scales.size(), size_t(3)); ++j) {
                if (j > 0) std::cout << ", ";
                const auto& s = sampler.scales[j];
                std::cout << "vec3(" << std::fixed << std::setprecision(2) 
                         << s.x << "," << s.y << "," << s.z << ")";
            }
            if (sampler.scales.size() > 3) {
                std::cout << ", ... (" << sampler.scales.size() << " scales)";
            }
            std::cout << "]\n";
        } else if (sampler.hasWeights()) {
            std::cout << "[";
            for (size_t j = 0; j < std::min(sampler.weights.size(), size_t(5)); ++j) {
                if (j > 0) std::cout << ", ";
                std::cout << std::fixed << std::setprecision(3) << sampler.weights[j];
            }
            if (sampler.weights.size() > 5) {
                std::cout << ", ... (" << sampler.weights.size() << " weights)";
            }
            std::cout << "]\n";
        } else {
            std::cout << "[empty]\n";
        }
    }
    
    // Print channels
    std::cout << "+-- Channels (" << channels.size() << " property bindings)\n";
    for (size_t i = 0; i < channels.size(); ++i) {
        const auto& channel = channels[i];
        std::string prefix = (i == channels.size() - 1) ? "    +-- " : "    +-- ";
        std::cout << prefix << "Channel[" << i << "]\n";
        
        std::string childPrefix = (i == channels.size() - 1) ? "        " : "    |   ";
        
        // Sampler index
        std::cout << childPrefix << "+-- Sampler Index: " << channel.samplerIndex << "\n";
        
        // Target node/bone
        std::cout << childPrefix << "+-- Target Bone: ";
        if (channel.targetBoneIndex >= 0) {
            if (!boneNames.empty() && channel.targetBoneIndex < static_cast<int>(boneNames.size())) {
                std::cout << "\"" << boneNames[channel.targetBoneIndex] << "\" [" << channel.targetBoneIndex << "]";
            } else {
                std::cout << "[" << channel.targetBoneIndex << "]";
            }
        } else {
            std::cout << "[Root/Model]";
        }
        std::cout << "\n";
        
        // Target path
        std::cout << childPrefix << "+-- Target Path: \"";
        switch (channel.targetPath) {
            case TinyAnimationChannel::TargetPath::Translation:
                std::cout << "translation";
                break;
            case TinyAnimationChannel::TargetPath::Rotation:
                std::cout << "rotation";
                break;
            case TinyAnimationChannel::TargetPath::Scale:
                std::cout << "scale";
                break;
            case TinyAnimationChannel::TargetPath::Weights:
                std::cout << "weights";
                break;
        }
        std::cout << "\"\n";
    }
    
    std::cout << std::string(60, '=') << "\n";
}