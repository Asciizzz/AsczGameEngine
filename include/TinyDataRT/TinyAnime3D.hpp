#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

#include "TinyExt/TinyPool.hpp"

struct TinySceneRT;
namespace TinyRT {

struct Anime3D {
    Anime3D() = default;

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
        TinyHandle node;
        uint32_t index = 0;
    };

    struct Anime {
        std::string name;
        std::vector<Sampler> samplers;
        std::vector<Channel> channels;
        float duration = 0.0f;
        bool valid() const { return !channels.empty() && !samplers.empty(); }
    };

    TinyHandle add(Anime&& anime) {
        if (!anime.valid()) return TinyHandle();

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

    void play(const std::string& name, bool restart = true);
    void play(const TinyHandle& handle, bool restart = true);
    void pause() { playing = false; }
    void resume() { playing = true; }
    void stop() { time = 0.0f; playing = false; }

    void update(TinySceneRT* scene, float deltaTime);

    Anime* current() { return animePool.get(currentHandle); }
    const Anime* current() const { return animePool.get(currentHandle); }

    Anime* get(const TinyHandle& handle) { return animePool.get(handle); }
    const Anime* get(const TinyHandle& handle) const { return animePool.get(handle); }

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

    // Retrieve the list
    const UnorderedMap<std::string, TinyHandle>& MAL() const {
        return nameToHandle;
    }

private:
    TinyPool<Anime> animePool;
    UnorderedMap<std::string, TinyHandle> nameToHandle;
    TinyHandle currentHandle;

    bool playing = false;
    bool loop = true;
    float time = 0.0f;
    float speed = 1.0f;

    void writeTransform(TinySceneRT* scene, const Channel& channel, const glm::mat4& transform) const;
};

} // namespace TinyRT

using TinyRT_AN3D = TinyRT::Anime3D;