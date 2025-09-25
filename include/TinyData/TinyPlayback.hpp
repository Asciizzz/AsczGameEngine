#pragma once

#include "TinyData/TinySkeleton.hpp"
#include "TinyData/TinyAnimation.hpp"

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

// Animation transition/blending state
struct BlendState {
    std::vector<BonePose> fromPose;  // Pose we're transitioning from
    float transitionTime = 0.0f;     // Current transition time
    float transitionDuration = 0.3f; // How long to blend (in seconds)
    bool blending = false;            // Whether we're currently blending
    
    void startTransition(const std::vector<BonePose>& currentPose, float duration = 0.3f) {
        fromPose = currentPose;
        transitionTime = 0.0f;
        transitionDuration = duration;
        blending = true;
    }
    
    void reset() {
        blending = false;
        transitionTime = 0.0f;
    }
    
    // Get blend factor (0.0 = fully fromPose, 1.0 = fully target)
    float getBlendFactor() const {
        if (!blending || transitionDuration <= 0.0f) return 1.0f;
        return glm::clamp(transitionTime / transitionDuration, 0.0f, 1.0f);
    }
    
    bool isBlending() const { return blending; }
};

class TinyPlayback {
public:
    const TinySkeleton* skeleton = nullptr;
    std::vector<BonePose> currentPose;      // Current pose for each bone
    std::vector<BonePose> bindPose;         // Default/bind pose for each bone
    std::vector<glm::mat4> boneMatrices;    // Final matrices for GPU
    
    AnimationState primaryState;  // Main animation
    BlendState blendState;         // Animation transition/blending

    // Initialize with a skeleton
    void setSkeleton(const TinySkeleton& skel);
    
    // Animation control
    void playAnimation(const TinyAnimation& anim, bool loop = true, float speed = 1.0f, float transitionTime = 0.3f);
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