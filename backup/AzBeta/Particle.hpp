#pragma once

#include "Tiny3D/TinyLoader.hpp"

#include <execution>
#include <algorithm>
#include <random>

namespace AzBeta {

class ParticleManager {
public:
    using StaticInstance = Az3D::StaticInstance;

    ParticleManager() = default;
    ~ParticleManager() = default;

    size_t modelIndex = 0;

    size_t particleCount = 0;
    std::vector<Az3D::Transform> particles; // Only store transforms, not full models
    std::vector<StaticInstance> particles_data;
    std::vector<glm::vec3> particles_velocity;
    std::vector<glm::vec3> particles_angular_velocity; // For rotation
    std::vector<short> particles_special; // Cool rare 1% drop particles
    std::vector<float> particles_rainbow; // Special scalar value for all special particles (not just rainbow)

    Az3D::StaticInstanceGroup instanceGroup;

    std::vector<glm::vec3> rainbow_colors = {
        glm::vec3(1.0f, 0.2f, 0.2f), // Red
        glm::vec3(1.0f, 0.5f, 0.2f), // Orange
        glm::vec3(1.0f, 1.0f, 0.2f), // Yellow
        glm::vec3(0.2f, 1.0f, 0.2f), // Green
        glm::vec3(0.2f, 0.2f, 1.0f), // Blue
        glm::vec3(0.5f, 0.2f, 1.0f)  // Purple
    };

    // 1% drop particles will have the following effects:
    // Index 1 - 33% chance to be red - immovable
    // Index 2 - 33% chance to be blue - perfect energy gain on collision
    // Index 3 - 33% chance to be green - 1% every frame to push/pull particles
    // Index 4 - 1% to be rainbow - immovable + x1.1 energy + 2% push
    // This meant a 0.01% chance to be rainbow lol

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
    float display_r = 0.05f; // for objects that seems bigger/smaller than their hitbox

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

    void initialize(Az3D::ResourceGroup* resGroup, const AzVulk::Device* deviceVK,
                    size_t count, float r = 0.05f, float display_r = 0.05f,
                    const glm::vec3& boundsMin = glm::vec3(-10.0f),
                    const glm::vec3& boundsMax = glm::vec3(10.0f)) {

        this->display_r = display_r;

        TinyModel particleModel;

        TinySubmesh submesh = TinyLoader::loadModel("Assets/Shapes/Icosphere.obj").submeshes[0];

        TinyTexture texture = TinyLoader::loadImage("Assets/Textures/Selen.png");

        TinyMaterial material;
        material.toonLevel = 2;
        material.albTexture = 0;

        instanceGroup.initVkDevice(deviceVK);
        instanceGroup.modelIndex = resGroup->addModel(particleModel);

        particleCount = count;
        radius = r;

        // Set up spatial grid with custom bounds
        spatialGrid.setBounds(boundsMin, boundsMax);

        particles.resize(count);
        particles_data.resize(count);

        particles_velocity.resize(count);
        particles_angular_velocity.resize(count);

        particles_special.resize(count, 0); // Initialize all to 0 (no special effect)
        particles_rainbow.resize(count, 0.0f); // Initialize all to 0 (no rainbow effect)

        // Calculate spawn area within bounds
        glm::vec3 spawnSize = boundsMax - boundsMin;
        glm::vec3 spawnCenter = boundsMin + spawnSize * 0.5f;
        glm::vec3 spawnArea = spawnSize * 0.8f; // Use 80% of the available space

        // Initialize particle transforms
        for (size_t i = 0; i < count; ++i) {
            particles[i].scale(display_r);
            particles[i].pos = spawnCenter + glm::vec3(
                (static_cast<float>(rand()) / RAND_MAX - 0.5f) * spawnArea.x,
                (static_cast<float>(rand()) / RAND_MAX - 0.5f) * spawnArea.y,
                (static_cast<float>(rand()) / RAND_MAX - 0.5f) * spawnArea.z
            );

            // Generate a number from 0 to 100
            int specialEffect = 0;

            // Cool way to only use 1 random number for 2 cases
            int randValue = rand() % 10000;
            // The lucky 1%
            if (randValue < 100) {
                if (randValue < 33) { specialEffect = 1; } else
                if (randValue < 66) { specialEffect = 2; } else
                if (randValue < 99) { specialEffect = 3; } else
                { specialEffect = 4; particles_rainbow[i] = rand(); }
            }

            particles_special[i] = specialEffect;

            particles_velocity[i] = randomDirection();

            // Generate data
            StaticInstance particleData;
            particleData.setTransform(particles[i].pos, particles[i].rot, display_r);
            particleData.multColor = glm::vec4(1.0f);

            particles_data[i] = particleData;

            instanceGroup.addInstance(particleData);
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

    void updateRender() {
        instanceGroup.datas = particles_data;
    }

    void updatePhysic(float dTime, const TinySubmesh* mesh, const glm::mat4& meshModelMat4) {
        std::vector<size_t> indices(particleCount);
        std::iota(indices.begin(), indices.end(), 0);

        glm::vec3 boundMin = spatialGrid.gridMin;
        glm::vec3 boundMax = spatialGrid.gridMax;

        /*

        // Parallel update of all particles
        std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t p) {
            // Gravity
            glm::vec3 &pos = particles[p].pos;
            glm::vec3 &vel = particles_velocity[p];

            // if (pos.y > 2.1f) vel.y -= 9.81f * dTime; // Simple gravity
            // else {
            //     vel *= 0.99f;

            //     vel.y += 1.0f * dTime; // Float on top of water
            // }
            vel.y -= 9.81f * dTime;

            float speed = glm::length(vel);
            glm::vec3 direction = glm::normalize(vel);

            float step = speed * dTime; // Limit step to "almost" diameter
            if (step > radius * 1.9f) step = radius * 1.9f;

            glm::vec3 predictedPos = pos + direction * step;

            Az3D::HitInfo map_collision = mesh->closestHit(
                predictedPos, radius, meshModelMat4
            );

            // Additional ray cast in case of an overstep
            Az3D::HitInfo ray_collision = mesh->closestHit(
                pos, direction, step, meshModelMat4
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

            float energyMultiplier = 0.8f;

            if (outAxisX || outAxisY || outAxisZ) {
                // Insanely cool branchless logic incoming

                vel = glm::reflect(direction, glm::vec3(
                    outLeft - outRight, outBottom - outTop, outBack - outFront
                ));
                vel *= speed * energyMultiplier;

                pos.x = pos.x * !outAxisX + minPlusRadius.x * outLeft + maxMinusRadius.x * outRight;
                pos.y = pos.y * !outAxisY + minPlusRadius.y * outBottom + maxMinusRadius.y * outTop;
                pos.z = pos.z * !outAxisZ + minPlusRadius.z * outBack + maxMinusRadius.z * outFront;
            } 

            if (map_collision.hit) {
                bool alreadyInside = map_collision.prop.z < radius;
                pos = alreadyInside ? (map_collision.vrtx + map_collision.nrml * radius) : pos;

                vel = glm::reflect(direction, map_collision.nrml);
                vel *= speed * energyMultiplier;
            } else {
                pos += direction * step;
            }
            
            // EXTREMELY WRONG, JUST TO LOOK COOL
            // Rotate the particles based on the direction and velocity

            // Rotation speed is affected by linear speed (hence, the extremely wrong)

            glm::quat rotation = glm::angleAxis(speed * dTime * 2.0f, direction);
            particles[p].rotate(rotation);
        });

        */

        // Handle particle-to-particle collisions after position updates
        handleParticleCollisions();

        std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t p) {
            glm::vec3 particleColor;
            switch (particles_special[p]) {
                // Default white
                case 0: particleColor = glm::vec3(1.0f, 1.0f, 1.0f); break;
                // 0.33% for unique rgb colors
                case 1: particleColor = glm::vec3(1.0f, 0.4f, 0.4f); break;
                case 2: particleColor = glm::vec3(0.4f, 0.4f, 1.0f); break;
                // If pull active, full green
                case 3: 
                    particleColor = particles_rainbow[p] > 0.0f ? // Push
                                    glm::vec3(5.0f, 10.0f, 0.0f) :
                                    particles_rainbow[p] < 0.0f ? // Pull
                                    glm::vec3(0.0f, 10.0f, 5.0f) :
                                    glm::vec3(0.4f, 1.0f, 0.4f); // No effect
                    break;
                    // 0.01% for rainbow
                case 4:
                    float speed = glm::length(particles_velocity[p]) + 1.0f; // Ensure the rainbow effect is always present

                    // 4th value will run from 0 -> 1 and mix 6 total color combination
                    float step = speed * dTime * 0.5f;

                    particles_rainbow[p] = fmodf(particles_rainbow[p] + step, 1.0f);
                    // Get the current process
                    int colorIndex = static_cast<int>(particles_rainbow[p] * rainbow_colors.size()) % rainbow_colors.size();
                    float local_w = particles_rainbow[p] * rainbow_colors.size() - static_cast<float>(colorIndex);

                    switch (colorIndex) {
                        case 0: particleColor = glm::mix(rainbow_colors[0], rainbow_colors[1], local_w); break;
                        case 1: particleColor = glm::mix(rainbow_colors[1], rainbow_colors[2], local_w); break;
                        case 2: particleColor = glm::mix(rainbow_colors[2], rainbow_colors[3], local_w); break;
                        case 3: particleColor = glm::mix(rainbow_colors[3], rainbow_colors[4], local_w); break;
                        case 4: particleColor = glm::mix(rainbow_colors[4], rainbow_colors[5], local_w); break;
                        case 5: particleColor = glm::mix(rainbow_colors[5], rainbow_colors[0], local_w); break;
                    }
                    break;
            }


            StaticInstance data;
            data.setTransform(particles[p].pos, particles[p].rot, display_r);
            data.multColor = glm::vec4(particleColor, 1.0f);

            particles_data[p] = data;
        });

    }


    static inline float radialPushSpeed(float distance) {
        const float maxRadius = 25.0f;
        const float maxSpeed = 2.0f;

        if (distance >= maxRadius) return 0.0f;

        float t = distance / maxRadius;  // t = [0..1]
        float falloff = 1.0f - t;        // Linear falloff

        return maxSpeed * falloff;
    }

    static inline float randomFloat(float min, float max) {
        static std::random_device rd;  // Only used once to seed the generator
        static std::mt19937 gen(rd()); // Mersenne Twister RNG
        std::uniform_real_distribution<float> dist(min, max);
        return dist(gen);
    }

};

}