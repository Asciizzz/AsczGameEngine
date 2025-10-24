#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <string>

#include "TinyExt/TinyHandle.hpp"

// Above are obsolete legacy structures kept for reference.

struct TinyScene;
struct TinyAnimeRT {
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
            T, R, S, W
        } path = Path::T;

        enum class Type {
            Node,
            Bone,
            Morph
        } type = Type::Node;

        // Will be remapped upon scene import
        TinyHandle node;
        uint32_t index = 0;
    };

    std::string name;
    std::vector<Sampler> samplers;
    std::vector<Channel> channels;

    float duration = 0.0f;
    float time = 0.0f;
    bool loop = true;

    void stop() { time = 0.0f; }
    bool valid() const { return !channels.empty() && !samplers.empty(); }

    glm::mat4 getTransform(const TinyScene* scene, const Channel& channel) const;
    void writeTransform(TinyScene* scene, const Channel& channel, const glm::mat4& transform) const;

    void update(TinyScene* scene, float deltaTime);
};