#pragma once

#include "Az3D/Az3D.hpp"
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

        // Spatial grid for efficient collision detection
        struct SpatialGrid {
            int resolution = 20; // Grid resolution
            float cellSize;
            glm::vec3 gridMin = glm::vec3(-86.0f, -10.0f, -77.0f);
            glm::vec3 gridMax = glm::vec3(163.0f, 132.0f, 92.0f);
            std::vector<std::vector<size_t>> cells;
            
            SpatialGrid() {
                glm::vec3 gridSize = gridMax - gridMin;
                cellSize = std::max({gridSize.x, gridSize.y, gridSize.z}) / resolution;
                cells.resize(resolution * resolution * resolution);
            }
            
            int getIndex(const glm::vec3& pos) const {
                glm::vec3 relPos = pos - gridMin;
                int x = std::clamp(int(relPos.x / cellSize), 0, resolution - 1);
                int y = std::clamp(int(relPos.y / cellSize), 0, resolution - 1);
                int z = std::clamp(int(relPos.z / cellSize), 0, resolution - 1);
                return x + y * resolution + z * resolution * resolution;
            }
            
            void clear() {
                for (auto& cell : cells) cell.clear();
            }
        } spatialGrid;

        float radius = 0.05f;
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

        void initParticles(size_t count, size_t modelResIdx, float r = 0.05f) {
            modelResourceIndex = modelResIdx;

            particleCount = count;
            radius = r;

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

        // Efficient particle-to-particle collision detection and response
        void handleParticleCollisions() {
            // Clear and populate spatial grid
            spatialGrid.clear();
            for (size_t i = 0; i < particleCount; ++i) {
                int cellIndex = spatialGrid.getIndex(particles[i].pos);
                spatialGrid.cells[cellIndex].push_back(i);
            }
            
            // Check collisions only within nearby cells
            for (size_t i = 0; i < particleCount; ++i) {
                glm::vec3 pos = particles[i].pos;
                int cellIndex = spatialGrid.getIndex(pos);
                
                // Check current cell and neighboring cells
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dz = -1; dz <= 1; dz++) {
                            glm::vec3 neighborPos = pos + glm::vec3(dx, dy, dz) * spatialGrid.cellSize;
                            int neighborIndex = spatialGrid.getIndex(neighborPos);
                            
                            if (neighborIndex >= 0 && neighborIndex < spatialGrid.cells.size()) {
                                for (size_t j : spatialGrid.cells[neighborIndex]) {
                                    if (j > i) { // Only check each pair once
                                        checkAndResolveCollision(i, j);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

    private:
        void checkAndResolveCollision(size_t i, size_t j) {
            glm::vec3 diff = particles[i].pos - particles[j].pos;
            float distance = glm::length(diff);
            float minDistance = radius * 2.0f;
            
            if (distance < minDistance && distance > 0.001f) {
                // Collision detected!
                glm::vec3 normal = diff / distance;
                float overlap = minDistance - distance;
                
                // Separate particles
                glm::vec3 separation = normal * (overlap * 0.5f);
                particles[i].pos += separation;
                particles[j].pos -= separation;
                
                // Calculate relative velocity
                glm::vec3 relativeVel = particles_velocity[i] - particles_velocity[j];
                float velAlongNormal = glm::dot(relativeVel, normal);
                
                // Don't resolve if velocities are separating
                if (velAlongNormal > 0) return;
                
                // Calculate impulse (assuming equal mass)
                float impulse = -(1.0f + restitution) * velAlongNormal * 0.5f;
                glm::vec3 impulseVec = impulse * normal;
                
                // Apply impulse
                particles_velocity[i] += impulseVec;
                particles_velocity[j] -= impulseVec;
            }
        }

    public:

        // Legacy separate functions for compatibility
        void addToRenderSystem(Az3D::RenderSystem& renderSystem) {
            for (size_t p = 0; p < particleCount; ++p) {

                // Get particle color based on direction
                glm::vec4 particleColor;

                // vec3 normal = normalize(fragWorldNrml);
                // vec3 normalColor = (normal + 1.0) * 0.5;

                glm::vec3 direction = glm::normalize(particles_velocity[p]);
                glm::vec3 normalColor = (direction + glm::vec3(1.0f)) * 0.5f; // Convert to [0, 1] range

                Az3D::ModelInstance instance;
                instance.modelMatrix() = particles[p].modelMatrix();
                instance.modelResourceIndex = modelResourceIndex;
                instance.multColor() = glm::vec4(normalColor, 1.0f);

                renderSystem.addInstance(instance);
            }
        }

        void update(float dTime, Az3D::Mesh& mesh, const Az3D::Transform& meshTransform) {
            std::vector<size_t> indices(particleCount);
            std::iota(indices.begin(), indices.end(), 0);

            glm::vec3 boundMin = glm::vec3(-86.0f, -10.0f, -77.0f);
            glm::vec3 boundMax = glm::vec3(163.0f, 132.0f, 92.0f);

            // Parallel update of all particles
            std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t p) {
                // Bound: (-86, -10, -77) -> (163, 32, 92)
                glm::vec3 nextPos = particles[p].pos + particles_velocity[p] * dTime;
                particles[p].pos.x = nextPos.x < boundMin.x ? boundMax.x : (nextPos.x > boundMax.x ? boundMin.x : particles[p].pos.x);
                particles[p].pos.y = nextPos.y < boundMin.y ? boundMax.y : (nextPos.y > boundMax.y ? boundMin.y : particles[p].pos.y);
                particles[p].pos.z = nextPos.z < boundMin.z ? boundMax.z : (nextPos.z > boundMax.z ? boundMin.z : particles[p].pos.z);

                // Gravity
                particles_velocity[p].y -= 9.81f * dTime; // Simple gravity

                float speed = glm::length(particles_velocity[p]);
                glm::vec3 direction = glm::normalize(particles_velocity[p]);

                float step = speed * dTime; // Limit step to diameter
                if (step > radius * 2.0f) step = radius * 2.0f;

                Az3D::HitInfo map_collision = mesh.closestHit(
                    particles[p].pos + direction * step,
                    radius,
                    meshTransform
                );

                if (map_collision.hit) {
                    // If distance smaller than radius, that means the particle is already inside, push it out
                    if (map_collision.prop.z < radius) {
                        particles[p].pos = map_collision.vrtx + map_collision.nrml * radius;
                    }

                    particles_velocity[p] = glm::reflect(direction, map_collision.nrml);
                    particles_velocity[p] *= speed * 0.8f;
                } else {
                    particles[p].pos += direction * step;
                }

            });
            
            // Handle particle-to-particle collisions after position updates
            handleParticleCollisions();
        }

    };

}