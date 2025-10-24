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