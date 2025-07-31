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

        void initParticles(size_t count, size_t meshIdx, size_t matIdx, float radius = 0.05f) {
            meshIndex = meshIdx;
            materialIndex = matIdx;

            particleCount = count;
            particleRadius = radius;

            float diameter = radius * 2.0f;

            particles.resize(count);
            particles_velocity.resize(count);

            // Link up with the models vector
            for (size_t i = 0; i < count; ++i) {
                particles[i].meshIndex = meshIdx;
                particles[i].materialIndex = matIdx;

                particles[i].trform.scale(radius); // Scale to diameter
                particles[i].trform.pos = glm::vec3(
                    static_cast<float>(rand()) / RAND_MAX * 20.0f - 10.0f,
                    static_cast<float>(rand()) / RAND_MAX * 20.0f - 10.0f,
                    static_cast<float>(rand()) / RAND_MAX * 20.0f - 10.0f
                );
                particles[i].trform.rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

                particles_velocity[i] = randomDirection();
            }
        }

        float particleRadius = 0.05f;
        size_t meshIndex = 0; // Mesh index for the particle model
        size_t materialIndex = 0; // Material index for the particle model

        size_t particleCount = 0;
        std::vector<Az3D::Model> particles;
        std::vector<glm::vec3> particles_velocity;

        void update(float dTime, Az3D::Mesh& mesh, AzBeta::Map& gameMap) {
            for (size_t p = 0; p < particleCount; ++p) {
                Az3D::Transform& trform = particles[p].trform;

                // If y is below a certain threshold, stop
                if (trform.pos.y < -20.0f) continue;

                // Gravity
                // particles_direction[p] -= glm::vec3(0.0f, 9.81f * dTime, 0.0f); // Simple gravity
                particles_velocity[p].y -= 9.81f * dTime; // Simple gravity

                float speed = glm::length(particles_velocity[p]);
                glm::vec3 direction = glm::normalize(particles_velocity[p]);

                float step = speed * dTime;
                if (step > particleRadius) step = particleRadius;

                HitInfo map_collision = gameMap.closestHit(
                    mesh,
                    trform.pos + direction * step,
                    particleRadius
                );

                if (map_collision.hit) {
                    // If distance smaller than radius, that means the particle is already inside, push it out
                    if (map_collision.prop.z < particleRadius) {
                        trform.pos = map_collision.vrtx + map_collision.nrml * particleRadius;
                    }

                    particles_velocity[p] = glm::reflect(direction, map_collision.nrml);

                    particles_velocity[p] *= speed * 0.8f;
                } else {
                    trform.pos += direction * step;
                }
            }
        }
    };

}