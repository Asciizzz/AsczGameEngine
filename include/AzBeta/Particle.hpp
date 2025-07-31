#pragma once

#include "Az3D/Az3D.hpp"

namespace AzBeta {

    class ParticleManager {
    public:
        ParticleManager() = default;
        ~ParticleManager() = default;

        void init(size_t count, size_t txtrIdx, float radius = 0.05f, float opacity = 0.4f) {
            textureIndex = txtrIdx;
            particleCount = count;
            particleRadius = radius;
            particleOpacity = opacity;

            float diameter = radius * 2.0f;

            particles.resize(count);
            particles_direction.resize(count);
            particles_velocity.resize(count);

            for (size_t i = 0; i < count; ++i) {
                // Having fun with random directions
                glm::vec3 rnd_direction = glm::normalize(glm::vec3(
                    static_cast<float>(rand()) / RAND_MAX - 0.5f,
                    static_cast<float>(rand()) / RAND_MAX - 0.5f,
                    static_cast<float>(rand()) / RAND_MAX - 0.5f
                ));

                glm::vec3 mult_direction = { 10.0f, 10.0f, 10.0f };

                particles_direction[i] = {
                    rnd_direction.x * mult_direction.x,
                    rnd_direction.y * mult_direction.y,
                    rnd_direction.z * mult_direction.z
                };

                // Position

                // glm::vec3 p_pos = { 2.5f, 15.2f, -45.3f};
                
                glm::vec3 p_pos = { 109.5f, 2.6f, -28.3f};

                particles[i] = Az3D::Billboard(
                    p_pos, diameter, txtrIdx,
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
                // Gravity
                // particles_direction[p] -= glm::vec3(0.0f, 9.81f * dTime, 0.0f); // Simple gravity
                particles_direction[p].y -= 9.81f * dTime; // Simple gravity

                float velocity = glm::length(particles_direction[p]);
                glm::vec3 direction = glm::normalize(particles_direction[p]);

                // Change color based on velocity (0.0 being cyan, 8.0 being red)
                float colorR = glm::clamp(velocity / 8.0f, 0.0f, 1.0f);
                float colorGB = 1.0f - colorR; // Inverse
                particles[p].color = glm::vec4(colorR, colorGB, colorGB, particleOpacity);

                float step = velocity * dTime;
                if (step > 0.1f) step = 0.1f; // Avoid lagspike causing tunneling

                HitInfo map_collision = gameMap.closestHit(
                    mesh,
                    particles[p].pos + direction * step,
                    particleRadius
                );

                if (map_collision.hit) {
                    particles_direction[p] = glm::reflect(direction, map_collision.nrml);

                    particles_direction[p].y *= 0.75f * velocity;
                } else {
                    particles[p].pos += direction * step;
                }
            }
        }
    };

}