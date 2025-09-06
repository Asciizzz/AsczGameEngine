#include "Az3D/TinyPlayback.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include <algorithm>
#include <cmath>

using namespace Az3D;

// ==================== BonePose Implementation ====================

glm::mat4 BonePose::toMatrix() const {
    glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
    glm::mat4 R = glm::toMat4(rotation);
    glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
    return T * R * S;
}

BonePose BonePose::lerp(const BonePose& a, const BonePose& b, float t) {
    BonePose result;
    result.translation = glm::mix(a.translation, b.translation, t);
    result.rotation = glm::slerp(a.rotation, b.rotation, t);
    result.scale = glm::mix(a.scale, b.scale, t);
    return result;
}

// ==================== TinyPlayback Implementation ====================

void TinyPlayback::setSkeleton(const TinySkeleton& skel) {
    skeleton = &skel;
    
    // Resize arrays to match skeleton
    currentPose.resize(skeleton->names.size());
    bindPose.resize(skeleton->names.size());
    boneMatrices.resize(skeleton->names.size());
    
    // Initialize bind pose from skeleton's local bind transforms
    for (size_t i = 0; i < skeleton->names.size(); ++i) {
        const glm::mat4& localTransform = skeleton->localBindTransforms[i];
        
        // Extract translation, rotation, scale from matrix
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        
        // Simple extraction - for more robust version, use glm::decompose
        translation = glm::vec3(localTransform[3]);
        
        // Extract scale
        scale.x = glm::length(glm::vec3(localTransform[0]));
        scale.y = glm::length(glm::vec3(localTransform[1]));
        scale.z = glm::length(glm::vec3(localTransform[2]));
        
        // Extract rotation (normalize the matrix first)
        glm::mat3 rotMatrix;
        rotMatrix[0] = glm::vec3(localTransform[0]) / scale.x;
        rotMatrix[1] = glm::vec3(localTransform[1]) / scale.y;
        rotMatrix[2] = glm::vec3(localTransform[2]) / scale.z;
        rotation = glm::quat_cast(rotMatrix);
        
        bindPose[i] = {translation, rotation, scale};
        currentPose[i] = bindPose[i];
    }
    
    computeBoneMatrices();
}

void TinyPlayback::playAnimation(const TinyAnimation& anim, bool loop, float speed) {
    primaryState.animation = &anim;
    primaryState.currentTime = 0.0f;
    primaryState.playing = true;
    primaryState.looping = loop;
    primaryState.speed = speed;
}

void TinyPlayback::stopAnimation() {
    primaryState.playing = false;
    primaryState.currentTime = 0.0f;
    resetToBindPose();
}

void TinyPlayback::pauseAnimation() {
    primaryState.playing = false;
}

void TinyPlayback::resumeAnimation() {
    if (primaryState.animation) {
        primaryState.playing = true;
    }
}

void TinyPlayback::update(float deltaTime) {
    if (!primaryState.playing || !primaryState.animation || !skeleton) {
        return;
    }
    
    // Update time
    primaryState.currentTime += deltaTime * primaryState.speed;

    // Handle looping
    if (primaryState.currentTime >= primaryState.animation->duration) {
        if (primaryState.looping) {
            primaryState.currentTime = std::fmod(primaryState.currentTime, primaryState.animation->duration);
        } else {
            primaryState.currentTime = primaryState.animation->duration;
            primaryState.playing = false;
        }
    }
    
    // Sample animation at current time
    sampleAnimation(*primaryState.animation, primaryState.currentTime, currentPose);
    
    // Compute final bone matrices
    computeBoneMatrices();
}

float TinyPlayback::getProgress() const {
    if (!primaryState.animation || primaryState.animation->duration <= 0.0f) {
        return 0.0f;
    }
    return primaryState.currentTime / primaryState.animation->duration;
}

void TinyPlayback::setAnimationTime(float time) {
    if (!primaryState.animation) return;
    
    primaryState.currentTime = std::max(0.0f, std::min(time, primaryState.animation->duration));
    
    if (skeleton) {
        sampleAnimation(*primaryState.animation, primaryState.currentTime, currentPose);
        computeBoneMatrices();
    }
}

void TinyPlayback::setAnimationProgress(float progress) {
    if (!primaryState.animation) return;
    
    float time = std::max(0.0f, std::min(progress, 1.0f)) * primaryState.animation->duration;
    setAnimationTime(time);
}

void TinyPlayback::sampleAnimation(const TinyAnimation& anim, float time, std::vector<BonePose>& outPose) {
    // Start with bind pose
    outPose = bindPose;
    
    // Apply each channel
    for (const auto& channel : anim.channels) {
        if (channel.targetBoneIndex < 0 || channel.targetBoneIndex >= static_cast<int>(outPose.size())) {
            continue; // Skip invalid bone indices
        }
        
        if (channel.samplerIndex < 0 || channel.samplerIndex >= static_cast<int>(anim.samplers.size())) {
            continue; // Skip invalid sampler indices
        }
        
        const auto& sampler = anim.samplers[channel.samplerIndex];
        BonePose& bonePose = outPose[channel.targetBoneIndex];
        
        // Sample the appropriate property
        switch (channel.targetPath) {
            case TinyAnimationChannel::TargetPath::Translation:
                bonePose.translation = interpolateTranslation(sampler, time);
                break;
            case TinyAnimationChannel::TargetPath::Rotation:
                bonePose.rotation = interpolateRotation(sampler, time);
                break;
            case TinyAnimationChannel::TargetPath::Scale:
                bonePose.scale = interpolateScale(sampler, time);
                break;
            case TinyAnimationChannel::TargetPath::Weights:
                // TODO: Handle morph target weights
                break;
        }
    }
}

void TinyPlayback::computeBoneMatrices() {
    if (!skeleton) return;
    
    // Compute world space transforms for each bone
    std::vector<glm::mat4> worldTransforms(skeleton->names.size());
    
    for (size_t i = 0; i < skeleton->names.size(); ++i) {
        glm::mat4 localTransform = currentPose[i].toMatrix();

        int parentIndex = skeleton->parentIndices[i];
        if (parentIndex >= 0) {
            // Multiply by parent's world transform
            worldTransforms[i] = worldTransforms[parentIndex] * localTransform;
        } else {
            // Root bone
            worldTransforms[i] = localTransform;
        }
        
        // Final bone matrix = world_transform * inverse_bind_matrix
        boneMatrices[i] = worldTransforms[i] * skeleton->inverseBindMatrices[i];
    }
}

void TinyPlayback::resetToBindPose() {
    currentPose = bindPose;
    computeBoneMatrices();
}

glm::vec3 TinyPlayback::interpolateTranslation(const TinyAnimationSampler& sampler, float time) {
    return interpolateValues(sampler.translations, sampler.inputTimes, time, sampler.interpolation);
}

glm::quat TinyPlayback::interpolateRotation(const TinyAnimationSampler& sampler, float time) {
    return interpolateValues(sampler.rotations, sampler.inputTimes, time, sampler.interpolation);
}

glm::vec3 TinyPlayback::interpolateScale(const TinyAnimationSampler& sampler, float time) {
    return interpolateValues(sampler.scales, sampler.inputTimes, time, sampler.interpolation);
}

template<typename T>
T TinyPlayback::interpolateValues(const std::vector<T>& values, const std::vector<float>& times, 
                                 float time, TinyAnimationSampler::InterpolationType interp) {
    if (values.empty() || times.empty()) {
        if constexpr (std::is_same_v<T, glm::vec3>) {
            return glm::vec3(0.0f);
        } else if constexpr (std::is_same_v<T, glm::quat>) {
            return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        }
    }
    
    if (values.size() == 1) { return values[0]; }
    
    // Find the keyframe interval
    auto it = std::lower_bound(times.begin(), times.end(), time);
    
    if (it == times.begin()) {
        // Before first keyframe
        return values[0];
    } else if (it == times.end()) {
        // After last keyframe
        return values.back();
    } else {
        // Between keyframes
        size_t nextIndex = it - times.begin();
        size_t prevIndex = nextIndex - 1;
        
        float t0 = times[prevIndex];
        float t1 = times[nextIndex];
        float factor = (time - t0) / (t1 - t0);
        
        const T& v0 = values[prevIndex];
        const T& v1 = values[nextIndex];
        
        switch (interp) {
            case TinyAnimationSampler::InterpolationType::Step:
                return v0;
            case TinyAnimationSampler::InterpolationType::Linear:
                if constexpr (std::is_same_v<T, glm::quat>) {
                    return glm::slerp(v0, v1, factor);
                } else {
                    return glm::mix(v0, v1, factor);
                }
            case TinyAnimationSampler::InterpolationType::CubicSpline:
                // TODO: Implement cubic spline interpolation
                // For now, fall back to linear
                if constexpr (std::is_same_v<T, glm::quat>) {
                    return glm::slerp(v0, v1, factor);
                } else {
                    return glm::mix(v0, v1, factor);
                }
            default:
                if constexpr (std::is_same_v<T, glm::quat>) {
                    return glm::slerp(v0, v1, factor);
                } else {
                    return glm::mix(v0, v1, factor);
                }
        }
    }
}

void TinyPlayback::printCurrentPose() const {
    if (!skeleton) {
        std::cout << "No skeleton set!\n";
        return;
    }
    
    std::cout << "Current Pose:\n";
    std::cout << std::string(50, '=') << "\n";
    
    for (size_t i = 0; i < currentPose.size(); ++i) {
        const auto& pose = currentPose[i];
        std::cout << "[" << i << "] " << skeleton->names[i] << ":\n";
        std::cout << "  Translation: (" << pose.translation.x << ", " << pose.translation.y << ", " << pose.translation.z << ")\n";
        std::cout << "  Rotation: (" << pose.rotation.w << ", " << pose.rotation.x << ", " << pose.rotation.y << ", " << pose.rotation.z << ")\n";
        std::cout << "  Scale: (" << pose.scale.x << ", " << pose.scale.y << ", " << pose.scale.z << ")\n";
    }
    
    std::cout << std::string(50, '=') << "\n";
}
