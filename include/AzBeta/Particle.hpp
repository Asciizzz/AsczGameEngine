#pragma once

#include "Az3D/Az3D.hpp"

namespace AzBeta {

    class ParticleManager {
    public:
        ParticleManager() = default;
        ~ParticleManager() = default;

        static inline glm::vec3 randomDirection() {
            return glm::normalize(glm::vec3(
                static_cast<float>(rand()) / RAND_MAX - 0.5f,
                static_cast<float>(rand()) / RAND_MAX - 0.5f,
                static_cast<float>(rand()) / RAND_MAX - 0.5f
            ));
        }

        void initParticles(size_t count, size_t txtrIdx, float radius = 0.05f, float opacity = 0.4f) {
            textureIndex = txtrIdx;
            particleCount = count;
            particleRadius = radius;
            particleOpacity = opacity;

            float diameter = radius * 2.0f;

            particles.resize(count);
            particles_direction.resize(count);
            particles_velocity.resize(count);

            for (size_t i = 0; i < count; ++i) {
                particles[i] = Az3D::Billboard(
                    glm::vec3(0.0f), diameter, txtrIdx,
                    glm::vec4(0.0f, 1.0f, 1.0f, opacity)
                );
            }
        }

        size_t textureIndex = 0;
        size_t particleCount = 0;
        float particleRadius = 0.05f;
        float particleOpacity = 0.4f;

        std::vector<Az3D::Billboard> particles;
        std::vector<glm::vec3> particles_direction;
        std::vector<float> particles_velocity;

        void update(float dTime, Az3D::Mesh& mesh, AzBeta::Map& gameMap) {
            for (size_t p = 0; p < particleCount; ++p) {
                // If y is below a certain threshold, stop
                if (particles[p].pos.y < -20.0f) continue;


                // Gravity
                // particles_direction[p] -= glm::vec3(0.0f, 9.81f * dTime, 0.0f); // Simple gravity
                particles_direction[p].y -= 9.81f * dTime; // Simple gravity

                float velocity = glm::length(particles_direction[p]);
                glm::vec3 direction = glm::normalize(particles_direction[p]);

                // Change color based on velocity (0.0 being cyan, 8.0 being red)
                float colorR = glm::clamp(velocity / 8.0f, 0.0f, 1.0f);
                float colorGB = 1.0f - colorR; // Inverse
                particles[p].color = glm::vec4(colorR, 0.0f, colorGB, particleOpacity);

                float step = velocity * dTime;
                if (step > particleRadius) step = particleRadius;

                HitInfo map_collision = gameMap.closestHit(
                    mesh,
                    particles[p].pos + direction * step,
                    particleRadius
                );

                if (map_collision.hit) {
                    particles_direction[p] = glm::reflect(direction, map_collision.nrml);

                    particles_direction[p] *= velocity * 0.8f;
                } else {
                    particles[p].pos += direction * step;
                }
            }
        }
    };

}