#include "TinyData/TinyScene.hpp"


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

glm::mat4 TinyAnimeRT::getTransform(const TinyScene* scene, const TinyAnimeRT::Channel& channel) const {
    if (scene == nullptr || channel.sampler >= samplers.size()) return glm::mat4(1.0f);

    // Return transform component of node
    if (channel.target == Channel::Target::Node) {
        const TinyNode::Transform* nodeTransform = scene->rtResolve<TinyNode::Transform>(channel.node);
        return nodeTransform ? nodeTransform->local : glm::mat4(1.0f);
    // Return transform component of bone
    } else if (channel.target == Channel::Target::Bone) {
        const TinySkeletonRT* skeletonRT = scene->rtResolve<TinyNode::Skeleton>(channel.node);
        return (skeletonRT && skeletonRT->boneValid(channel.index)) ? skeletonRT->localPose(channel.index) : glm::mat4(1.0f);
    }

    return glm::mat4(1.0f);
}

void TinyAnimeRT::writeTransform(TinyScene* scene, const Channel& channel, const glm::mat4& transform) const {
    if (scene == nullptr) return;

    // Write transform component of node
    if (channel.target == Channel::Target::Node) {
        TinyNode::Transform* nodeTransform = scene->rtResolve<TinyNode::Transform>(channel.node);
        if (nodeTransform) nodeTransform->local = transform;
    // Write transform component of bone
    } else if (channel.target == Channel::Target::Bone) {
        TinySkeletonRT* skeletonRT = scene->rtResolve<TinyNode::Skeleton>(channel.node);
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
    glm::quat rotation = glm::quat_cast(original);

    glm::vec3 scale;
    scale.x = glm::length(glm::vec3(original[0]));
    scale.y = glm::length(glm::vec3(original[1]));
    scale.z = glm::length(glm::vec3(original[2]));

    // Replace with new components if provided
    if (newTranslation) translation = *newTranslation;
    if (newRotation)    rotation = *newRotation;
    if (newScale)       scale = *newScale;

    // Recompose transform
    return glm::translate(glm::mat4(1.0f), translation) *
           glm::mat4_cast(rotation) *
           glm::scale(glm::mat4(1.0f), scale);
}

void TinyAnimeRT::update(TinyScene* scene, float deltaTime) {
    if (scene == nullptr || !valid()) return;

    time += deltaTime;
    if (loop) time = fmod(time, duration);
    else      time = std::min(time, duration);

    for (auto& channel : channels) {
        auto& sampler = samplers[channel.sampler];

        glm::mat4 transform = getTransform(scene, channel);
        glm::vec4 value = sampler.evaluate(time);

        switch (channel.path) {
            case Channel::Path::T: { // Translation
                glm::vec3 t(value.x, value.y, value.z);
                transform = recomposeTransform(transform, &t, nullptr, nullptr);
                break;
            }
            case Channel::Path::R: { // Rotation (quaternion stored in vec4)
                glm::quat r(value.w, value.x, value.y, value.z); // Note the order (w, x, y, z)
                transform = recomposeTransform(transform, nullptr, &r, nullptr);
                break;
            }
            case Channel::Path::S: { // Scale
                glm::vec3 s(value.x, value.y, value.z);
                transform = recomposeTransform(transform, nullptr, nullptr, &s);
                break;
            }
        }

        writeTransform(scene, channel, transform);
    }
}