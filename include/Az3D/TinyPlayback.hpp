#pragma once

#include "Az3D/TinySkeleton.hpp"
#include "Az3D/TinyAnimation.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace Az3D {

// Represents the current pose of a single bone
struct BonePose {
    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};  // w, x, y, z
    glm::vec3 scale{1.0f};
    
    // Convert to transformation matrix
    glm::mat4 toMatrix() const;
    
    // Blend with another pose
    static BonePose lerp(const BonePose& a, const BonePose& b, float t);
};

// Current animation state
struct AnimationState {
    const TinyAnimation* animation = nullptr;
    float currentTime = 0.0f;
    bool playing = false;
    bool looping = true;
    float speed = 1.0f;  // Animation speed multiplier
    float weight = 1.0f; // For blending (future use)
    
    void reset() {
        currentTime = 0.0f;
        playing = false;
    }
};

class TinyPlayback {
public:
    const TinySkeleton* skeleton = nullptr;
    std::vector<BonePose> currentPose;      // Current pose for each bone
    std::vector<BonePose> bindPose;         // Default/bind pose for each bone
    std::vector<glm::mat4> boneMatrices;    // Final matrices for GPU
    
    AnimationState primaryState;  // Main animation

    // Initialize with a skeleton
    void setSkeleton(const TinySkeleton& skel);
    
    // Animation control
    void playAnimation(const TinyAnimation& anim, bool loop = true, float speed = 1.0f);
    void stopAnimation();
    void pauseAnimation();
    void resumeAnimation();
    
    // Update the animation (call every frame)
    void update(float deltaTime);
    
    // Get results
    const std::vector<glm::mat4>& getBoneMatrices() const { return boneMatrices; }
    const std::vector<BonePose>& getCurrentPose() const { return currentPose; }
    
    // Animation state queries
    bool isPlaying() const { return primaryState.playing; }
    float getCurrentTime() const { return primaryState.currentTime; }
    float getDuration() const { return primaryState.animation ? primaryState.animation->duration : 0.0f; }
    float getProgress() const; // Returns 0.0-1.0
    
    // Manual control
    void setAnimationTime(float time);
    void setAnimationProgress(float progress); // 0.0-1.0
    
    // Debug
    void printCurrentPose() const;
    
private:
    // Core animation processing
    void sampleAnimation(const TinyAnimation& anim, float time, std::vector<BonePose>& outPose);
    void computeBoneMatrices();
    void resetToBindPose();
    
    // Interpolation helpers
    glm::vec3 interpolateTranslation(const TinyAnimationSampler& sampler, float time);
    glm::quat interpolateRotation(const TinyAnimationSampler& sampler, float time);
    glm::vec3 interpolateScale(const TinyAnimationSampler& sampler, float time);
    
    // Generic interpolation for any type
    template<typename T>
    T interpolateValues(const std::vector<T>& values, const std::vector<float>& times, 
                        float time, TinyAnimationSampler::InterpolationType interp);
};

}