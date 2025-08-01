#pragma once

#include "Az3D/Az3D.hpp"
#include "Az3D/RenderingSystem.hpp"
#include <execution>
#include <algorithm>

namespace AzBeta {

    // Use the HitInfo from Az3D namespace
    using HitInfo = Az3D::HitInfo;

    class ParticleManager {
    public:
        ParticleManager() = default;
        ~ParticleManager() = default;

        size_t modelResourceIndex = 0; // Index into the global rendering system's model resources

        size_t particleCount = 0;
        std::vector<Az3D::Transform> particles; // Only store transforms, not full models
        std::vector<glm::vec3> particles_velocity;
        std::vector<glm::vec3> particles_angular_velocity; // For rotation

        float particleRadius = 0.05f;
        const float mass = 1.0f;          // Mass of the particle
        const float restitution = 0.6f;   // Bounciness (0=no bounce, 1=perfect bounce)
        const float friction = 0.4f;      // How much the surface "grabs" the ball

        // Helper function to generate a random direction vector
        static inline glm::vec3 randomDirection() {
            return glm::normalize(glm::vec3(
                static_cast<float>(rand()) / RAND_MAX - 0.5f,
                static_cast<float>(rand()) / RAND_MAX - 0.5f,
                static_cast<float>(rand()) / RAND_MAX - 0.5f
            ));
        }

        void initParticles(size_t count, size_t modelResIdx, float radius = 0.05f) {
            modelResourceIndex = modelResIdx;

            particleCount = count;
            particleRadius = radius;

            particles.resize(count);
            particles_velocity.resize(count);
            particles_angular_velocity.resize(count);

            // Initialize particle transforms
            for (size_t i = 0; i < count; ++i) {
                particles[i].scale(radius); // Scale to radius
                particles[i].pos = glm::vec3(
                    static_cast<float>(rand()) / RAND_MAX * 20.0f - 10.0f,
                    static_cast<float>(rand()) / RAND_MAX * 20.0f - 10.0f,
                    static_cast<float>(rand()) / RAND_MAX * 20.0f - 10.0f
                );

                particles_velocity[i] = randomDirection();
            }
        }

        // Legacy separate functions for compatibility
        void addToRenderSystem(Az3D::RenderSystem& renderSystem) {
            for (const auto& particle : particles) {
                renderSystem.addInstance(particle.modelMatrix(), modelResourceIndex);
            }
        }

        void update(float dTime, Az3D::Mesh& mesh, const Az3D::Transform& meshTransform) {
            std::vector<size_t> indices(particleCount);
            std::iota(indices.begin(), indices.end(), 0);

            // Parallel update of all particles
            std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t p) {
                // If y is below a certain threshold, skip update but still render
                if (particles[p].pos.y > -20.0f) {
                    // Gravity
                    particles_velocity[p].y -= 9.81f * dTime; // Simple gravity

                    float speed = glm::length(particles_velocity[p]);
                    if (speed > 0.001f) { // Avoid division by zero
                        glm::vec3 direction = glm::normalize(particles_velocity[p]);

                        float step = speed * dTime;
                        if (step > particleRadius) step = particleRadius;

                        Az3D::HitInfo map_collision = mesh.closestHit(
                            particles[p].pos + direction * step,
                            particleRadius,
                            meshTransform
                        );

                        if (map_collision.hit) {
                            // If distance smaller than radius, that means the particle is already inside, push it out
                            if (map_collision.prop.z < particleRadius) {
                                particles[p].pos = map_collision.vrtx + map_collision.nrml * particleRadius;
                            }

                            particles_velocity[p] = glm::reflect(direction, map_collision.nrml);
                            particles_velocity[p] *= speed * 0.8f;
                        } else {
                            particles[p].pos += direction * step;
                        }
                    }
                }
            });
        }

    };

}