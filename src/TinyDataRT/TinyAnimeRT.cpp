#include "TinyDataRT/TinySceneRT.hpp"

glm::vec4 TinyAnimeRT::Sampler::firstKeyframe() const {
    if (values.empty()) return glm::vec4(0.0f);
    return (interp == Interp::CubicSpline && values.size() >= 3) ? values[1] : values[0];
}
glm::vec4 TinyAnimeRT::Sampler::lastKeyframe() const {
    if (values.empty()) return glm::vec4(0.0f);
    return (interp == Interp::CubicSpline && values.size() >= 3) ? values[values.size() - 2] : values.back();
}

glm::vec4 TinyAnimeRT::Sampler::evaluate(float time) const {
    if (times.empty() || values.empty()) return glm::vec4(0.0f);

    const float tMin = times.front();
    const float tMax = times.back();

    // Clamp time within the keyframe range
    if (time <= tMin) return firstKeyframe();
    if (time >= tMax) return lastKeyframe();

    // Find the appropriate keyframe interval using binary search
    size_t left = 0;
    size_t right = times.size() - 1;
    while (left < right - 1) {
        size_t mid = left + (right - left) / 2;

        if (time < times[mid]) right = mid;
        else                   left = mid;
    }
    size_t index = left;

    // Linear interpolation between keyframes
    float t0 = times[index];
    float t1 = times[index + 1];

    // Prevent division by zero
    float dt = std::max(t1 - t0, 1e-6f);

    const float f = (time - t0) / dt;
    switch (interp) {
        case Interp::Linear: {
            const glm::vec4& v0 = values[index];
            const glm::vec4& v1 = values[index + 1];
            return glm::mix(v0, v1, f);
        }

        case Interp::Step: {
            return values[index];
        }

        case Interp::CubicSpline: {
            // Each keyframe: [inTangent, value, outTangent]

            const size_t idx0 = index * 3;
            const size_t idx1 = (index + 1) * 3;

            if (idx1 + 1 >= values.size()) return values[idx0 + 1]; // Fallback

            const glm::vec4& in0  = values[idx0];
            const glm::vec4& v0   = values[idx0 + 1];
            const glm::vec4& out0 = values[idx0 + 2];

            const glm::vec4& in1  = values[idx1];
            const glm::vec4& v1   = values[idx1 + 1];
            const glm::vec4& out1 = values[idx1 + 2];

            float f2 = f * f;
            float f3 = f2 * f;

            // Hermite basis functions
            float h00 = 2.0f * f3 - 3.0f * f2 + 1.0f;
            float h10 = f3 - 2.0f * f2 + f;
            float h01 = -2.0f * f3 + 3.0f * f2;
            float h11 = f3 - f2;

            glm::vec4 m0 = out0 * dt;
            glm::vec4 m1 = in1 * dt;

            return h00 * v0 + h10 * m0 + h01 * v1 + h11 * m1;
        }

        default:
            return glm::vec4(0.0f);
    }
}



void TinyAnimeRT::play(const std::string& name, bool restart) {
    auto it = nameToHandle.find(name);
    if (it != nameToHandle.end()) {
        play(it->second, restart);
    }
}

void TinyAnimeRT::play(const TinyHandle& handle, bool restart) {
    const Anime* anim = animePool.get(handle);
    if (!anim || !anim->valid()) return;
    
    playing = true;
    currentHandle = handle;

    if (restart) time = 0.0f;
}


glm::mat4 getTransform(const TinySceneRT* scene, const TinyAnimeRT::Channel& channel) {
    if (scene == nullptr) return glm::mat4(1.0f);
    using AnimeTarget = TinyAnimeRT::Channel::Target;

    // Return transform component of node
    if (channel.target == AnimeTarget::Node) {
        const TinyNodeRT::T3D* nodeTransform = scene->rtComp<TinyNodeRT::T3D>(channel.node);
        return nodeTransform ? nodeTransform->local : glm::mat4(1.0f);
    // Return transform component of bone
    } else if (channel.target == AnimeTarget::Bone) {
        const TinySkeletonRT* skeletonRT = scene->rtComp<TinyNodeRT::SK3D>(channel.node);
        return skeletonRT->bindPose(channel.index);
    }

    return glm::mat4(1.0f);
}


void TinyAnimeRT::writeTransform(TinySceneRT* scene, const Channel& channel, const glm::mat4& transform) const {
    if (scene == nullptr) return;

    // Write transform component of node
    if (channel.target == Channel::Target::Node) {
        TinyNodeRT::T3D* nodeTransform = scene->rtComp<TinyNodeRT::T3D>(channel.node);
        if (nodeTransform) {
            nodeTransform->set(transform);
            scene->updateTransform(channel.node);
        }
    // Write transform component of bone
    } else if (channel.target == Channel::Target::Bone) {
        TinySkeletonRT* skeletonRT = scene->rtComp<TinyNodeRT::SK3D>(channel.node);
        if (skeletonRT && skeletonRT->boneValid(channel.index)) {
            skeletonRT->setLocalPose(channel.index, transform);
        }
    }
}


glm::mat4 recomposeTransform(
    const glm::mat4& original,
    const glm::vec3* newTranslation = nullptr,
    const glm::quat* newRotation = nullptr,
    const glm::vec3* newScale = nullptr
) {
    // Extract existing components
    glm::vec3 translation = glm::vec3(original[3]);

    // Extract scale from column lengths
    glm::vec3 scale;
    scale.x = glm::length(glm::vec3(original[0]));
    scale.y = glm::length(glm::vec3(original[1]));
    scale.z = glm::length(glm::vec3(original[2]));

    // To extract rotation correctly, build a rotation-only matrix by removing scale
    glm::mat3 rotMat;
    if (scale.x > 1e-6f && scale.y > 1e-6f && scale.z > 1e-6f) {
        rotMat[0] = glm::vec3(original[0]) / scale.x;
        rotMat[1] = glm::vec3(original[1]) / scale.y;
        rotMat[2] = glm::vec3(original[2]) / scale.z;
    } else {
        // fallback: if scale nearly zero, just use the matrix's upper-left 3x3
        rotMat[0] = glm::vec3(original[0]);
        rotMat[1] = glm::vec3(original[1]);
        rotMat[2] = glm::vec3(original[2]);
    }

    glm::quat rotation = glm::quat_cast(rotMat);
    // Apply replacements if provided
    if (newTranslation) translation = *newTranslation;
    if (newRotation)    rotation = *newRotation;
    if (newScale)       scale = *newScale;

    // Ensure quaternion normalized
    rotation = glm::normalize(rotation);

    // Recompose transform: T * R * S (R from quat to mat4)
    return glm::translate(glm::mat4(1.0f), translation) *
           glm::mat4_cast(rotation) *
           glm::scale(glm::mat4(1.0f), scale);
}


void TinyAnimeRT::update(TinySceneRT* scene, float deltaTime) {
    if (scene == nullptr) return;

    const Anime* anime = animePool.get(currentHandle);
    if (!playing || !anime || !anime->valid()) return;

    float duration = anime->duration;

    time += deltaTime * speed;
    if (duration <= 0.0f) {
        // Zero-length animation: clamp to start
        time = 0.0f;
    } else if (loop) {
        time = fmod(time, duration);
        if (time < 0.0f) time += duration; // handle negative deltaTime safely
    } else {
        time = std::min(time, duration);
    }

    for (const auto& channel : anime->channels) {
        const auto& sampler = anime->samplers[channel.sampler];

        glm::mat4 transform = getTransform(scene, channel);

        // Evaluate value (for T and S we can use sampler.evaluate).
        // Rotation needs special handling (slerp).
        switch (channel.path) {
            case Channel::Path::T: { // Translation
                glm::vec4 v = sampler.evaluate(time);
                glm::vec3 t(v.x, v.y, v.z);
                transform = recomposeTransform(transform, &t, nullptr, nullptr);
                break;
            }

            case Channel::Path::S: { // Scale
                glm::vec4 v = sampler.evaluate(time);
                glm::vec3 s(v.x, v.y, v.z);
                transform = recomposeTransform(transform, nullptr, nullptr, &s);
                break;
            }

            case Channel::Path::R: { // Rotation (special: slerp)
                // Handle edge cases quickly
                if (sampler.times.empty() || sampler.values.empty()) break;

                // Clamp
                float tMin = sampler.times.front();
                float tMax = sampler.times.back();
                if (time <= tMin) {
                    glm::vec4 v = sampler.firstKeyframe();
                    glm::quat q(v.w, v.x, v.y, v.z);
                    q = glm::normalize(q);
                    transform = recomposeTransform(transform, nullptr, &q, nullptr);
                    break;
                }
                if (time >= tMax) {
                    glm::vec4 v = sampler.lastKeyframe();
                    glm::quat q(v.w, v.x, v.y, v.z);
                    q = glm::normalize(q);
                    transform = recomposeTransform(transform, nullptr, &q, nullptr);
                    break;
                }

                // Binary search interval
                size_t left = 0;
                size_t right = sampler.times.size() - 1;
                while (left < right - 1) {
                    size_t mid = left + (right - left) / 2;
                    if (time < sampler.times[mid]) right = mid;
                    else left = mid;
                }
                size_t index = left;
                float t0 = sampler.times[index];
                float t1 = sampler.times[index + 1];
                float dt = std::max(t1 - t0, 1e-6f);
                float f = (time - t0) / dt;

                // Read quaternions: assume values store (x,y,z,w) or (w,x,y,z) depending on your importer.
                // Here we assume stored as (x,y,z,w). If your importer stores w first, invert below.
                const glm::vec4& vv0 = sampler.values[index];
                const glm::vec4& vv1 = sampler.values[index + 1];

                // Two common conventions:
                // - glTF rotation is (x, y, z, w) — typical. Many code paths also use (w, x, y, z).
                // We'll assume glTF (x,y,z,w) by default. If you used w-first earlier, swap the order.
                glm::quat q0 = glm::quat(vv0.w, vv0.x, vv0.y, vv0.z); // (w,x,y,z)
                glm::quat q1 = glm::quat(vv1.w, vv1.x, vv1.y, vv1.z);

                // If your stored order was (x,y,z,w), the above is correct because glm::quat ctor expects (w,x,y,z)
                // if your stored order is (w,x,y,z) change to: glm::quat q0(vv0.x, vv0.y, vv0.z, vv0.w);

                q0 = glm::normalize(q0);
                q1 = glm::normalize(q1);

                glm::quat out;
                if (sampler.interp == Sampler::Interp::Step) {
                    out = q0;
                } else {
                    // For Linear (and fallback for CubicSpline), slerp between q0 and q1
                    out = glm::slerp(q0, q1, f);
                    out = glm::normalize(out);
                }

                transform = recomposeTransform(transform, nullptr, &out, nullptr);
                break;
            }
        } // switch

        // finally write back
        writeTransform(scene, channel, transform);
    } // channels
}