#pragma once

#include "Az3D/Az3D.hpp"
#include "Az3D/RenderingSystem.hpp"

namespace AzBeta {

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

        // Add particles to rendering system
        void addToRenderSystem(Az3D::RenderSystem& renderSystem) {
            for (const auto& particle : particles) {
                renderSystem.addInstance(particle.modelMatrix(), modelResourceIndex);
            }
        }

        /*
        void update(float dTime, Az3D::Mesh& mesh, AzBeta::Map& gameMap) {
            for (size_t p = 0; p < particleCount; ++p) {
                Az3D::Transform& trform = particles[p].trform;

                // If y is below a certain threshold, stop
                if (trform.pos.y < -20.0f) continue;

                // Gravity
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
        */

        void update(float dTime, Az3D::Mesh& mesh, AzBeta::Map& gameMap) {
            if (particles_angular_velocity.size() != particleCount) {
                particles_angular_velocity.resize(particleCount, glm::vec3(0.0f));
            }

            for (size_t p = 0; p < particleCount; ++p) {
                Az3D::Transform& trform = particles[p];
                glm::vec3& velocity = particles_velocity[p];
                glm::vec3& angular_velocity = particles_angular_velocity[p];

                if (trform.pos.y < -20.0f) continue;

                // Apply global gravity (for when in the air)
                velocity.y -= 9.81f * dTime;
                
                float step = glm::length(velocity) * dTime;
                if (step > particleRadius) step = particleRadius;

                HitInfo map_collision = gameMap.closestHit(
                    mesh,
                    trform.pos + step * glm::normalize(velocity),
                    particleRadius
                );

                if (map_collision.hit) {
                    if (map_collision.prop.z < particleRadius) {
                        trform.pos = map_collision.vrtx + map_collision.nrml * particleRadius;
                    }

                    glm::vec3 n = map_collision.nrml;

                    // FIX #1: Apply gravity force parallel to the slope to make the ball roll down.
                    glm::vec3 gravity_force = glm::vec3(0.0f, -9.81f * mass, 0.0f);
                    glm::vec3 slope_gravity = gravity_force - glm::dot(gravity_force, n) * n;
                    velocity += (slope_gravity / mass) * dTime;

                    // Decompose velocity for response
                    float v_dot_n = glm::dot(velocity, n);
                    glm::vec3 v_normal = v_dot_n * n;
                    glm::vec3 v_tangent = velocity - v_normal;

                    // Linear Response (Bounce + Kinetic Friction)
                    velocity = (v_tangent * (1.0f - friction)) - (v_normal * restitution);

                    // Angular Response (Torque from Kinetic Friction)
                    if (glm::length(v_tangent) > 0.0f) {
                        float inertia = (2.0f / 5.0f) * mass * particleRadius * particleRadius;
                        glm::vec3 r = -n * particleRadius;
                        glm::vec3 friction_impulse = v_tangent * friction * mass;
                        glm::vec3 torque = glm::cross(r, friction_impulse);
                        angular_velocity += torque / inertia;
                    }

                    // FIX #2: Apply static friction to stop movement and rolling when at rest.
                    if (glm::length(velocity) < 0.02f) {
                        float static_friction_damping = 0.2f; // High damping factor
                        velocity *= (1.0f - static_friction_damping);
                        angular_velocity *= (1.0f - static_friction_damping);
                    }
                }
                
                // --- Integration ---
                // Update position and rotation
                trform.pos += velocity * dTime;
                if (glm::length(angular_velocity) > 0.001f) {
                    trform.rot = glm::angleAxis(glm::length(angular_velocity) * dTime, glm::normalize(angular_velocity)) * trform.rot;
                    trform.rot = glm::normalize(trform.rot);
                }
            }
        }

    };

}