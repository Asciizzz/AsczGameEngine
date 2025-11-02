#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

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

    struct Anime {
        std::string name;
        std::vector<Sampler> samplers;
        std::vector<Channel> channels;
        float duration = 0.0f;
        bool valid() const { return !channels.empty() && !samplers.empty(); }
    };

    tinyHandle add(Anime&& anime) {
        if (!anime.valid()) return tinyHandle();

        std::string baseName = anime.name.empty() ? "Anime" : anime.name;
        std::string uniqueName = baseName;
        int suffix = 1;

        // Ensure unique name
        while (nameToHandle.find(uniqueName) != nameToHandle.end()) {
            uniqueName = baseName + "_" + std::to_string(suffix++);
        }
        anime.name = uniqueName;

        // Cache duration
        for (const auto& sampler : anime.samplers) {
            if (sampler.times.empty()) continue;
            anime.duration = std::max(anime.duration, sampler.times.back());
        }

        nameToHandle[uniqueName] = animePool.add(std::move(anime));

        return nameToHandle[uniqueName];
    }

    bool isPlaying() const { return playing; }
    void play(const std::string& name, bool restart = true);
    void play(const tinyHandle& handle, bool restart = true);
    void pause() { playing = false; }
    void resume() { playing = true; }
    void stop() { time = 0.0f; playing = false; }

    // Direct time manipulation for editor scrubbing
    void setTime(float newTime) { time = newTime; }
    float getTime() const { return time; }
    
    // Speed control (can be negative for backward playback)
    void setSpeed(float newSpeed) { speed = newSpeed; }
    float getSpeed() const { return speed; }
    
    // Loop control
    void setLoop(bool shouldLoop) { loop = shouldLoop; }
    bool getLoop() const { return loop; }
    
    // Apply animation at current time to the scene (for manual scrubbing)
    void apply(Scene* scene, const tinyHandle& animeHandle);
    
    void update(Scene* scene, float deltaTime);

    Anime* current() { return animePool.get(currentHandle); }
    const Anime* current() const { return animePool.get(currentHandle); }
    
    tinyHandle currentAnimHandle() const { return currentHandle; }

    Anime* get(const tinyHandle& handle) { return animePool.get(handle); }
    const Anime* get(const tinyHandle& handle) const { return animePool.get(handle); }

    Anime* get(const std::string& name) {
        auto it = nameToHandle.find(name);
        if (it != nameToHandle.end()) {
            return animePool.get(it->second);
        }
        return nullptr;
    }
    const Anime* get(const std::string& name) const {
        return const_cast<Anime3D*>(this)->get(name);
    }

    const std::deque<Anime>& all() const {
        return animePool.view();
    }

    // Retrieve the list
    const UnorderedMap<std::string, tinyHandle>& MAL() const {
        return nameToHandle;
    }

private:
    tinyPool<Anime> animePool;
    UnorderedMap<std::string, tinyHandle> nameToHandle;
    tinyHandle currentHandle;

    bool playing = false;
    bool loop = true;
    float time = 0.0f;
    float speed = 1.0f;

    void writeTransform(Scene* scene, const Channel& channel, const glm::mat4& transform);
};

} // namespace tinyRT

using tinyRT_ANIM3D = tinyRT::Anime3D;