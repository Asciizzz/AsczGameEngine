#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

#include "Templates.hpp"

#include "tinyPool.hpp"

namespace tinyRT {

struct Scene; // Forward declaration

struct Anime3D {
    Anime3D() noexcept = default;

    // Allows copy since its just data
    Anime3D(const Anime3D&) = default;
    Anime3D& operator=(const Anime3D&) = default;

    Anime3D(Anime3D&&) noexcept = default;
    Anime3D& operator=(Anime3D&&) noexcept = default;

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

        enum class Target {
            Node,
            Bone,
            Morph
        } target = Target::Node;

        // Will be remapped upon scene import
        tinyHandle node;
        uint32_t index = 0;
    };

    struct Clip {
        std::string name;
        std::vector<Sampler> samplers;
        std::vector<Channel> channels;
        float duration = 0.0f;
        bool valid() const { return !channels.empty() && !samplers.empty(); }
    };

    tinyHandle add(Clip&& clip) {
        if (!clip.valid()) return tinyHandle();

        std::string baseName = clip.name.empty() ? "Clip" : clip.name;
        std::string uniqueName = baseName;
        int suffix = 1;

        // Ensure unique name
        while (nameToHandle.find(uniqueName) != nameToHandle.end()) {
            uniqueName = baseName + "_" + std::to_string(suffix++);
        }
        clip.name = uniqueName;

        // Cache duration
        for (const auto& sampler : clip.samplers) {
            if (sampler.times.empty()) continue;
            clip.duration = std::max(clip.duration, sampler.times.back());
        }

        nameToHandle[uniqueName] = clips.emplace(std::move(clip));

        return nameToHandle[uniqueName];
    }

    bool isPlaying() const { return playing; }
    void play(const std::string& name, bool restart = true);
    void play(const tinyHandle& handle, bool restart = true);
    void pause() { playing = false; }
    void resume() { playing = true; }
    void stop() { time = 0.0f; playing = false; }
    
    // Set current animation without starting playback
    void setCurrent(const tinyHandle& handle, bool resetTime = true) {
        const Clip* clip = clips.get(handle);
        if (!clip || !clip->valid()) return;
        currentHandle = handle;
        if (resetTime) time = 0.0f;
    }

    void setTime(float newTime) { time = newTime; }
    float getTime() const { return time; }

    void setSpeed(float newSpeed) { speed = newSpeed; }
    float getSpeed() const { return speed; }

    void setLoop(bool shouldLoop) { loop = shouldLoop; }
    bool getLoop() const { return loop; }

    float duration(tinyHandle handle) const {
        const Clip* clip = clips.get(handle);
        return clip ? clip->duration : 0.0f;
    }
    float duration(const std::string& name) const {
        auto it = nameToHandle.find(name);
        if (it == nameToHandle.end()) return 0.0f;
        return duration(it->second);
    }
    
    // Apply animation at current time to the scene (for manual scrubbing)
    void apply(Scene* scene, const tinyHandle& animeHandle);
    
    void update(Scene* scene, float deltaTime);

    Clip* current() { return clips.get(currentHandle); }
    const Clip* current() const { return clips.get(currentHandle); }

    tinyHandle curHandle() const { return currentHandle; }

    Clip* get(const tinyHandle& handle) { return clips.get(handle); }
    const Clip* get(const tinyHandle& handle) const { return clips.get(handle); }

    Clip* get(const std::string& name) {
        auto it = nameToHandle.find(name);
        if (it != nameToHandle.end()) {
            return clips.get(it->second);
        }
        return nullptr;
    }
    const Clip* get(const std::string& name) const {
        return const_cast<Anime3D*>(this)->get(name);
    }

    tinyHandle getHandle(const std::string& name) const {
        auto it = nameToHandle.find(name);
        if (it == nameToHandle.end()) return tinyHandle();
        return it->second;
    }

    const std::deque<Clip>& all() const {
        return clips.data();
    }

    // Retrieve the list
    const UnorderedMap<std::string, tinyHandle>& MAL() const {
        return nameToHandle;
    }

private:
    tinyPool<Clip> clips;
    UnorderedMap<std::string, tinyHandle> nameToHandle;
    tinyHandle currentHandle;

    bool playing = false;
    bool loop = true;
    float time = 0.0f;
    float speed = 1.0f;
};

} // namespace tinyRT

using tinyRT_ANIM3D = tinyRT::Anime3D;