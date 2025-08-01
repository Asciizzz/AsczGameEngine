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
            int resolution = 12; // Sweet spot for performance vs quality
            float cellSize;
            float invCellSize; // Pre-computed inverse for faster division
            glm::vec3 gridMin;
            glm::vec3 gridMax;
            glm::ivec3 gridDimensions; // Store as integers for faster access
            std::vector<std::vector<size_t>> cells;
            
            // Cache for commonly accessed values
            int totalCells;
            int resolutionSquared;
            
            SpatialGrid() {
                // Default bounds - will be updated when particles are initialized
                gridMin = glm::vec3(-86.0f, -10.0f, -77.0f);
                gridMax = glm::vec3(163.0f, 132.0f, 92.0f);
                updateGrid();
            }
            
            void updateGrid() {
                glm::vec3 gridSize = gridMax - gridMin;
                cellSize = std::max({gridSize.x, gridSize.y, gridSize.z}) / resolution;
                invCellSize = 1.0f / cellSize; // Pre-compute inverse
                
                // Cache frequently used values
                gridDimensions = glm::ivec3(resolution);
                totalCells = resolution * resolution * resolution;
                resolutionSquared = resolution * resolution;
                
                cells.clear();
                cells.resize(totalCells);
                
                // Pre-reserve based on expected particle density
                int avgParticlesPerCell = 8;
                for (auto& cell : cells) {
                    cell.reserve(avgParticlesPerCell);
                }
            }
            
            void setBounds(const glm::vec3& min, const glm::vec3& max) {
                gridMin = min;
                gridMax = max;
                updateGrid();
            }
            
            // Optimized index calculation using pre-computed values
            inline int getIndex(const glm::vec3& pos) const {
                glm::vec3 relPos = (pos - gridMin) * invCellSize; // Use pre-computed inverse
                int x = std::clamp(static_cast<int>(relPos.x), 0, resolution - 1);
                int y = std::clamp(static_cast<int>(relPos.y), 0, resolution - 1);
                int z = std::clamp(static_cast<int>(relPos.z), 0, resolution - 1);
                return x + y * resolution + z * resolutionSquared; // Use cached value
            }
            
            // Get grid coordinates directly (useful for neighbor iteration)
            inline glm::ivec3 getGridCoords(const glm::vec3& pos) const {
                glm::vec3 relPos = (pos - gridMin) * invCellSize;
                return glm::ivec3(
                    std::clamp(static_cast<int>(relPos.x), 0, resolution - 1),
                    std::clamp(static_cast<int>(relPos.y), 0, resolution - 1),
                    std::clamp(static_cast<int>(relPos.z), 0, resolution - 1)
                );
            }
            
            // Convert grid coordinates to linear index
            inline int coordsToIndex(const glm::ivec3& coords) const {
                return coords.x + coords.y * resolution + coords.z * resolutionSquared;
            }
            
            // Check if coordinates are valid
            inline bool isValidCoord(const glm::ivec3& coords) const {
                return coords.x >= 0 && coords.x < resolution &&
                       coords.y >= 0 && coords.y < resolution &&
                       coords.z >= 0 && coords.z < resolution;
            }
            
            void clear() {
                for (auto& cell : cells) cell.clear();
            }
            
            // Get all particles in neighboring cells (including current cell)
            void getNeighboringParticles(const glm::vec3& pos, std::vector<size_t>& neighbors) const {
                neighbors.clear();
                glm::ivec3 centerCoords = getGridCoords(pos);
                
                // Check 3x3x3 neighborhood
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dz = -1; dz <= 1; dz++) {
                            glm::ivec3 neighborCoords = centerCoords + glm::ivec3(dx, dy, dz);
                            if (isValidCoord(neighborCoords)) {
                                int neighborIndex = coordsToIndex(neighborCoords);
                                const auto& cell = cells[neighborIndex];
                                neighbors.insert(neighbors.end(), cell.begin(), cell.end());
                            }
                        }
                    }
                }
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

        void initParticles( size_t count, size_t modelResIdx, float r = 0.05f, 
                            const glm::vec3& boundsMin = glm::vec3(-86.0f, -10.0f, -77.0f),
                            const glm::vec3& boundsMax = glm::vec3(163.0f, 132.0f, 92.0f)) {
            modelResourceIndex = modelResIdx;

            particleCount = count;
            radius = r;

            // Set up spatial grid with custom bounds
            spatialGrid.setBounds(boundsMin, boundsMax);

            particles.resize(count);
            particles_velocity.resize(count);
            particles_angular_velocity.resize(count);

            // Calculate spawn area within bounds
            glm::vec3 spawnSize = boundsMax - boundsMin;
            glm::vec3 spawnCenter = boundsMin + spawnSize * 0.5f;
            glm::vec3 spawnArea = spawnSize * 0.8f; // Use 80% of the available space

            // Initialize particle transforms
            for (size_t i = 0; i < count; ++i) {
                particles[i].scale(radius); // Scale to radius
                particles[i].pos = spawnCenter + glm::vec3(
                    (static_cast<float>(rand()) / RAND_MAX - 0.5f) * spawnArea.x,
                    (static_cast<float>(rand()) / RAND_MAX - 0.5f) * spawnArea.y,
                    (static_cast<float>(rand()) / RAND_MAX - 0.5f) * spawnArea.z
                );

                particles_velocity[i] = randomDirection();
            }
        }

        // Optimized particle-to-particle collision detection and response
        void handleParticleCollisions() {
            // Clear and populate spatial grid
            spatialGrid.clear();
            for (size_t i = 0; i < particleCount; ++i) {
                int cellIndex = spatialGrid.getIndex(particles[i].pos);
                spatialGrid.cells[cellIndex].push_back(i);
            }
            
            const float radiusSquared = radius * radius * 4.0f; // Pre-compute (2*radius)^2
            
            // Process only non-empty cells to avoid wasted iterations
            for (int cellIdx = 0; cellIdx < spatialGrid.totalCells; ++cellIdx) {
                const auto& currentCell = spatialGrid.cells[cellIdx];
                if (currentCell.empty()) continue;
                
                // Get cell coordinates once
                int z = cellIdx / spatialGrid.resolutionSquared;
                int y = (cellIdx % spatialGrid.resolutionSquared) / spatialGrid.resolution;
                int x = cellIdx % spatialGrid.resolution;
                
                // Check particles within current cell (self-collisions)
                for (size_t i = 0; i < currentCell.size(); ++i) {
                    for (size_t j = i + 1; j < currentCell.size(); ++j) {
                        checkAndResolveCollisionFast(currentCell[i], currentCell[j], radiusSquared);
                    }
                }
                
                // Check only forward neighbors to avoid duplicate checks
                static const int neighborOffsets[13][3] = {
                    {1, 0, 0}, {0, 1, 0}, {0, 0, 1},    // 6-connectivity
                    {1, 1, 0}, {1, -1, 0}, {1, 0, 1}, {1, 0, -1},  // edges
                    {0, 1, 1}, {0, 1, -1}, {1, 1, 1}, {1, 1, -1}, {1, -1, 1}, {1, -1, -1}  // corners
                };
                
                for (const auto& offset : neighborOffsets) {
                    int nx = x + offset[0];
                    int ny = y + offset[1];
                    int nz = z + offset[2];
                    
                    if (nx >= 0 && nx < spatialGrid.resolution &&
                        ny >= 0 && ny < spatialGrid.resolution &&
                        nz >= 0 && nz < spatialGrid.resolution) {
                        
                        int neighborIdx = nx + ny * spatialGrid.resolution + nz * spatialGrid.resolutionSquared;
                        const auto& neighborCell = spatialGrid.cells[neighborIdx];
                        
                        // Cross-cell collisions
                        for (size_t i : currentCell) {
                            for (size_t j : neighborCell) {
                                checkAndResolveCollisionFast(i, j, radiusSquared);
                            }
                        }
                    }
                }
            }
        }

    private:
        // Ultra-optimized collision check using SIMD-friendly operations
        void checkAndResolveCollisionFast(size_t i, size_t j, float radiusSquared) {
            // Manual vectorization for better performance
            const glm::vec3& pos1 = particles[i].pos;
            const glm::vec3& pos2 = particles[j].pos;
            
            float dx = pos1.x - pos2.x;
            float dy = pos1.y - pos2.y;
            float dz = pos1.z - pos2.z;
            float distanceSquared = dx * dx + dy * dy + dz * dz;
            
            if (distanceSquared < radiusSquared && distanceSquared > 0.0001f) {
                // Fast inverse square root approximation for initial guess
                float invDistance = 1.0f / std::sqrt(distanceSquared);
                float distance = distanceSquared * invDistance;
                
                // Normal calculation
                float nx = dx * invDistance;
                float ny = dy * invDistance;
                float nz = dz * invDistance;
                
                // Separation
                float overlap = radius * 2.0f - distance;
                float separationHalf = overlap * 0.5f;
                
                particles[i].pos.x += nx * separationHalf;
                particles[i].pos.y += ny * separationHalf;
                particles[i].pos.z += nz * separationHalf;
                
                particles[j].pos.x -= nx * separationHalf;
                particles[j].pos.y -= ny * separationHalf;
                particles[j].pos.z -= nz * separationHalf;
                
                // Velocity resolution
                glm::vec3& vel1 = particles_velocity[i];
                glm::vec3& vel2 = particles_velocity[j];
                
                float relVelX = vel1.x - vel2.x;
                float relVelY = vel1.y - vel2.y;
                float relVelZ = vel1.z - vel2.z;
                
                float velAlongNormal = relVelX * nx + relVelY * ny + relVelZ * nz;
                
                if (velAlongNormal > 0) return; // Separating
                
                float impulse = -(1.0f + restitution) * velAlongNormal * 0.5f;
                float impulseX = impulse * nx;
                float impulseY = impulse * ny;
                float impulseZ = impulse * nz;
                
                vel1.x += impulseX;
                vel1.y += impulseY;
                vel1.z += impulseZ;
                
                vel2.x -= impulseX;
                vel2.y -= impulseY;
                vel2.z -= impulseZ;
            }
        }
        
        // Keep the old function for fallback
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

                // Get particle color based on speed
                // Gradient: Blue 0 -> Yellow 3 -> Red 10
                

                float speed = glm::length(particles_velocity[p]);
                speed = glm::clamp(speed, 0.0f, 10.0f);

                glm::vec3 particleColor;

                if (speed < 3.0f) {
                    particleColor = glm::mix(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 1.0f, 0.0f), speed / 3.0f);
                } else {
                    particleColor = glm::mix(glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), (speed - 3.0f) / 7.0f);
                }

                Az3D::ModelInstance instance;
                instance.modelMatrix() = particles[p].modelMatrix();
                instance.modelResourceIndex = modelResourceIndex;
                instance.multColor() = glm::vec4(particleColor, 1.0f);

                renderSystem.addInstance(instance);
            }
        }

        void update(float dTime, Az3D::Mesh& mesh, const Az3D::Transform& meshTransform) {
            std::vector<size_t> indices(particleCount);
            std::iota(indices.begin(), indices.end(), 0);

            glm::vec3 boundMin = spatialGrid.gridMin;
            glm::vec3 boundMax = spatialGrid.gridMax;

            // Parallel update of all particles
            std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t p) {
                // Gravity
                glm::vec3 &pos = particles[p].pos;
                glm::vec3 &vel = particles_velocity[p];

                vel.y -= 9.81f * dTime; // Simple gravity

                float speed = glm::length(vel);
                glm::vec3 direction = glm::normalize(vel);

                float step = speed * dTime; // Limit step to "almost" diameter
                if (step > radius * 1.9f) step = radius * 1.9f;

                glm::vec3 predictedPos = pos + direction * step;

                Az3D::HitInfo map_collision = mesh.closestHit(
                    predictedPos, radius, meshTransform
                );
                // Additional ray cast in case of an overstep
                Az3D::HitInfo ray_collision = mesh.closestHit(
                    pos, direction, step, meshTransform
                );

                // Check if predicted pos is inside the bounding box
                glm::vec3 minPlusRadius = boundMin + glm::vec3(radius);
                glm::vec3 maxMinusRadius = boundMax - glm::vec3(radius);

                bool outLeft = predictedPos.x < minPlusRadius.x;
                bool outRight = predictedPos.x > maxMinusRadius.x;
                bool outAxisX = outLeft || outRight;

                bool outBottom = predictedPos.y < minPlusRadius.y;
                bool outTop = predictedPos.y > maxMinusRadius.y;
                bool outAxisY = outBottom || outTop;

                bool outBack = predictedPos.z < minPlusRadius.z;
                bool outFront = predictedPos.z > maxMinusRadius.z;
                bool outAxisZ = outBack || outFront;

                if (outAxisX || outAxisY || outAxisZ) {
                    // Insanely cool branchless logic incoming

                    vel = glm::reflect(direction, glm::vec3(
                        outLeft - outRight, outBottom - outTop, outBack - outFront
                    ));
                    vel *= speed * 0.8f;

                    pos.x = pos.x * !outAxisX + minPlusRadius.x * outLeft + maxMinusRadius.x * outRight;
                    pos.y = pos.y * !outAxisY + minPlusRadius.y * outBottom + maxMinusRadius.y * outTop;
                    pos.z = pos.z * !outAxisZ + minPlusRadius.z * outBack + maxMinusRadius.z * outFront;
                } 

                if (map_collision.hit) {
                    bool alreadyInside = map_collision.prop.z < radius;
                    pos = alreadyInside ? (map_collision.vrtx + map_collision.nrml * radius) : pos;

                    vel = glm::reflect(direction, map_collision.nrml);
                    vel *= speed * 0.8f; // Energy loss
                } else if (ray_collision.hit) { // Edge case
                    // Place it directly on the normal
                    pos = ray_collision.vrtx + ray_collision.nrml * radius;
                } else {
                    pos += direction * step;
                }

            });

            // Handle particle-to-particle collisions after position updates
            handleParticleCollisions();
        }

    };

}