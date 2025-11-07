#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <functional>

#include ".ext/Templates.hpp"

#include "tinyExt/tinyPool.hpp"

namespace tinyRT {

struct Scene; // Forward declaration

struct Anime3D {
    Anime3D() noexcept = default;

    // Allows copy since its just data
    Anime3D(const Anime3D&) = default;
    Anime3D& operator=(const Anime3D&) = default;

    Anime3D(Anime3D&&) noexcept = default;
    Anime3D& operator=(Anime3D&&) noexcept = default;

    // ============================================================
    // POSE REPRESENTATION (Intermediate Animation State)
    // ============================================================

    struct Pose {
        // Generic target identifier (matches Channel system)
        struct TargetID {
            tinyHandle node; uint32_t index = 0;
            enum class Type { Node, Bone, Morph } type = Type::Node;

            bool operator==(const TargetID& other) const {
                return node == other.node && index == other.index && type == other.type;
            }
            bool operator!=(const TargetID& other) const {
                return !(*this == other);
            }
        };

        struct TargetIDHash {
            size_t operator()(const TargetID& id) const {
                return std::hash<uint64_t>()((uint64_t)id.node.value ^ ((uint64_t)id.index << 32) ^ (uint64_t)id.type);
            }
        };

        struct Transform {
            glm::vec3 translation = glm::vec3(0.0f);
            glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            glm::vec3 scale = glm::vec3(1.0f);
            float morphWeight = 0.0f;
        };

        UnorderedMap<TargetID, Transform, TargetIDHash> transforms;

        // Blend two poses linearly
        static Pose blend(const Pose& a, const Pose& b, float t);
        // Additive blending (result = base + (additive - reference) * weight)
        static Pose blendAdditive(const Pose& base, const Pose& additive, const Pose& reference, float weight);

        void applyToScene(Scene* scene) const;
    };

    // ============================================================
    // CORE ANIMATION DATA STRUCTURES
    // ============================================================

    struct Sampler {
        std::vector<float> times;
        std::vector<glm::vec4> values;
        enum class Interp {
            Linear,     // One vec4 per keyframe
            Step,       // Same
            CubicSpline // triplets [inTangent, value, outTangent] 
        } interp = Interp::Linear;

        glm::vec4 firstKeyframe() const;
        glm::vec4 lastKeyframe() const;
        glm::vec4 evaluate(float time) const;
    };

    struct Channel {
        uint32_t sampler = 0;

        enum class Path {
            T, R, S, W  // Translation, Rotation, Scale, Weight (morph)
        } path = Path::T;

        enum class Target {
            Node,   // Scene node transform
            Bone,   // Skeleton bone
            Morph   // Mesh morph target
        } target = Target::Node;

        tinyHandle node;      // Target node in scene
        uint32_t index = 0;   // Bone/morph index
    };

    struct Clip {
        std::string name;
        std::vector<Sampler> samplers;
        std::vector<Channel> channels;
        float duration = 0.0f;
        
        bool valid() const { return !channels.empty() && !samplers.empty(); }
        
        // Evaluate clip at specific time and return a Pose
        Pose evaluatePose(float time) const;
    };

    // ============================================================
    // STATE MACHINE (Animation State & Transitions)
    // ============================================================

    struct State {
        std::string name;
        tinyHandle clipHandle;
        float speed = 1.0f;
        bool loop = true;
        float timeOffset = 0.0f;
    };

    struct Transition {
        tinyHandle fromState;
        tinyHandle toState;
        float duration = 0.3f;

        enum class Type {
            Smooth,       // Crossfade blend
            Frozen,       // Hold source pose
            Synchronized  // Match normalized time
        } type = Type::Smooth;

        std::function<bool()> condition = nullptr;
        
        bool exitTime = false;
        float exitTimeNormalized = 1.0f;
    };

    class StateMachine {
    public:
        tinyHandle addState(const std::string& name, const tinyHandle& clipHandle);
        tinyHandle addState(State&& state);
        void removeState(const tinyHandle& handle);
        void addTransition(Transition&& transition);
        
        void setCurrentState(const tinyHandle& stateHandle, bool resetTime = true);
        tinyHandle currentState() const { return currentState_; }
        
        State* getState(const tinyHandle& handle);
        const State* getState(const tinyHandle& handle) const;
        
        State* getState(const std::string& name);
        const State* getState(const std::string& name) const;
        
        tinyHandle getStateHandle(const std::string& name) const;
        
        // Direct state iteration (name -> handle map)
        const UnorderedMap<std::string, tinyHandle>& stateNameMap() const { return nameToHandle_; }
        
        // Evaluate current state/transition and return pose
        Pose evaluate(Anime3D* animData, float deltaTime);
        
        // Check and trigger transitions
        void evaluateTransitions();
        
        // Playback control
        bool isPlaying() const { return isPlaying_; }
        void setPlaying(bool playing) { isPlaying_ = playing; }
        
        float currentTime() const { return currentTime_; }
        void setCurrentTime(float time) { currentTime_ = time; }
        
        bool isTransitioning() const { return activeTransition_ != nullptr; }
        
    private:
        tinyPool<State> states_;
        UnorderedMap<std::string, tinyHandle> nameToHandle_;
        std::vector<Transition> transitions_;
        
        tinyHandle currentState_;
        tinyHandle nextState_;
        float currentTime_ = 0.0f;
        bool isPlaying_ = false;
        
        float transitionProgress_ = 0.0f;
        Transition* activeTransition_ = nullptr;
    };

    // ============================================================
    // LAYER SYSTEM (Multi-Animation Blending)
    // ============================================================

    struct Layer {
        std::string name;
        float weight = 1.0f;
        
        enum class BlendMode {
            Override,  // Replace lower layers
            Additive   // Add to lower layers
        } blendMode = BlendMode::Override;

        // Mask: which targets this layer affects
        struct TargetMask {
            UnorderedSet<Pose::TargetID, Pose::TargetIDHash> includedTargets;
            bool invertMask = false;
            
            bool affects(const Pose::TargetID& target) const {
                if (includedTargets.empty()) return true; // No mask = affect all
                bool isIncluded = includedTargets.count(target) > 0;
                return invertMask ? !isIncluded : isIncluded;
            }
        } mask;

        StateMachine stateMachine;
        Pose referencePose; // For additive blending
    };

    // ============================================================
    // CONTROLLER (Main Animation System)
    // ============================================================

    class Controller {
    public:
        Controller() = default;

        Layer& addLayer(const std::string& name = "");
        void removeLayer(const std::string& name);
        void removeLayer(size_t index);
        
        void setLayerWeight(const std::string& name, float weight);
        float getLayerWeight(const std::string& name) const;
        
        Layer* getLayer(const std::string& name);
        const Layer* getLayer(const std::string& name) const;
        
        Layer* getLayer(size_t index);
        const Layer* getLayer(size_t index) const;
        
        StateMachine* getStateMachine(const std::string& layerName = "Base");
        const StateMachine* getStateMachine(const std::string& layerName = "Base") const;
        
        // Main update
        void update(Anime3D* animData, Scene* scene, float deltaTime);
        
        // Manual pose application
        void applyPose(Scene* scene, const Pose& pose);
        
        // Direct layer access
        std::vector<Layer>& layers() { return layers_; }
        const std::vector<Layer>& layers() const { return layers_; }
        
        size_t layerCount() const { return layers_.size(); }
        
    private:
        std::vector<Layer> layers_;
        
        Pose evaluateFinal(Anime3D* animData, float deltaTime);
        Pose blendLayers(const std::vector<Pose>& layerPoses);
    };

    // ============================================================
    // ACCESSORS & MANAGEMENT
    // ============================================================

    tinyHandle addClip(Clip&& clip);
    
    Clip* getClip(const tinyHandle& handle);
    const Clip* getClip(const tinyHandle& handle) const;
    
    Clip* getClip(const std::string& name);
    const Clip* getClip(const std::string& name) const;
    
    tinyHandle getClipHandle(const std::string& name) const;
    
    float clipDuration(const tinyHandle& handle) const;
    float clipDuration(const std::string& name) const;
    
    const std::deque<Clip>& allClips() const { return clips_.view(); }
    const UnorderedMap<std::string, tinyHandle>& clipNameMap() const { return clipNameToHandle_; }

    Controller& controller() { return controller_; }
    const Controller& controller() const { return controller_; }

private:
    tinyPool<Clip> clips_;
    UnorderedMap<std::string, tinyHandle> clipNameToHandle_;
    Controller controller_;
};

} // namespace tinyRT

using tinyRT_ANIM3D = tinyRT::Anime3D;