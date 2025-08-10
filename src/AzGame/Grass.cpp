#include "AzGame/Grass.hpp"

#include "Az3D/Az3D.hpp"
#include "AzVulk/AzVulk.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>

using namespace AzGame;
using namespace Az3D;
using namespace AzVulk;

WindGrassInstance::WindGrassInstance(const glm::mat4& transform, const glm::vec4& instanceColor, 
                                    float baseHeight, float flexibility, float phaseOffset) {
    modelMatrix = transform;
    color = instanceColor;
    windData = glm::vec4(baseHeight, flexibility, phaseOffset, 0.0f);
}

Grass::Grass(const GrassConfig& grassConfig) : config(grassConfig) {
    heightMap.resize(config.worldSizeX, std::vector<float>(config.worldSizeZ, 0.0f));
}

Grass::~Grass() {

}

bool Grass::initialize(ResourceManager& resourceManager) {
    createGrassMesh(resourceManager);

    std::mt19937 generator(std::random_device{}());
    generateHeightMap(generator);
    generateTerrainMesh(resourceManager);

    printf("Grass system initialized with %d height nodes, terrain scale %.2f, height scale %.2f\n",
           config.numHeightNodes, terrainScale, heightScale);

    generateGrassInstances(generator);

    for (size_t i = 0; i < grassInstances.size(); ++i) {
        grassInstanceUpdateQueue.push_back(i);
    }

    grassModelGroup.addModelResource("Grass", grassMeshIndex, grassMaterialIndex);
    grassModelGroup.addInstances(grassInstances);
    grassModelGroup.buildMeshMapping();

    terrainModelGroup.addModelResource("Terrain", terrainMeshIndex, terrainMaterialIndex);
    terrainModelGroup.addInstances(terrainInstances);
    terrainModelGroup.buildMeshMapping();

    printf("Grass system initialized successfully with %zu grass instances!\n", grassInstances.size());
    return true;
}

void Grass::generateHeightMap(std::mt19937& generator) {
    // Clear existing height map
    for (auto& row : heightMap) {
        std::fill(row.begin(), row.end(), 0.0f);
    }
    
    // Generate height map using random nodes with smooth falloff
    std::uniform_int_distribution<int> rnd_x_node(0, config.worldSizeX - 1);
    std::uniform_int_distribution<int> rnd_z_node(0, config.worldSizeZ - 1);
    std::uniform_real_distribution<float> rnd_height(-config.lowVariance, config.heightVariance);
    
    for (int i = 0; i < config.numHeightNodes; ++i) {
        int centerX = rnd_x_node(generator);
        int centerZ = rnd_z_node(generator);
        float nodeHeight = rnd_height(generator);
        
        // Apply height to center node and surrounding area with smooth falloff
        int radiusInt = static_cast<int>(std::ceil(config.falloffRadius));
        
        for (int x = std::max(0, centerX - radiusInt); 
            x < std::min(config.worldSizeX, centerX + radiusInt + 1); ++x) {
            for (int z = std::max(0, centerZ - radiusInt); 
                z < std::min(config.worldSizeZ, centerZ + radiusInt + 1); ++z) {
                float distance = std::sqrt((x - centerX) * (x - centerX) + (z - centerZ) * (z - centerZ));
                
                if (distance <= config.falloffRadius) {
                    // Smooth falloff using cosine interpolation for natural curves
                    float normalizedDistance = distance / config.falloffRadius * 0.5f;
                    float influence = 0.2f * (1.0f + std::cos(normalizedDistance * glm::pi<float>()));
                    heightMap[x][z] += nodeHeight * influence;
                }
            }
        }
    }
}

void Grass::createGrassMesh(Az3D::ResourceManager& resourceManager) {
    // Create grass geometry - 3 intersecting quads for volume
    glm::vec3 g_normal(0.0f, 1.0f, 0.0f);

    glm::vec2 g_uv00(0.0f, 0.0f);
    glm::vec2 g_uv10(1.0f, 0.0f);
    glm::vec2 g_uv11(1.0f, 1.0f);
    glm::vec2 g_uv01(0.0f, 1.0f);

    glm::vec3 g_pos1(-0.5f, 0.0f, 0.0f);
    glm::vec3 g_pos2(0.5f, 0.0f, 0.0f);
    glm::vec3 g_pos3(0.5f, 1.0f, 0.0f);
    glm::vec3 g_pos4(-0.5f, 1.0f, 0.0f);

    // Rotate for 3 intersecting quads
    glm::vec3 g_pos5 = Transform::rotate(g_pos1, g_normal, glm::radians(120.0f));
    glm::vec3 g_pos6 = Transform::rotate(g_pos2, g_normal, glm::radians(120.0f));
    glm::vec3 g_pos7 = Transform::rotate(g_pos3, g_normal, glm::radians(120.0f));
    glm::vec3 g_pos8 = Transform::rotate(g_pos4, g_normal, glm::radians(120.0f));

    glm::vec3 g_pos9 = Transform::rotate(g_pos1, g_normal, glm::radians(240.0f));
    glm::vec3 g_pos10 = Transform::rotate(g_pos2, g_normal, glm::radians(240.0f));
    glm::vec3 g_pos11 = Transform::rotate(g_pos3, g_normal, glm::radians(240.0f));
    glm::vec3 g_pos12 = Transform::rotate(g_pos4, g_normal, glm::radians(240.0f));

    std::vector<Vertex> grassVertices = {
        {g_pos1, g_normal, g_uv01},
        {g_pos2, g_normal, g_uv11},
        {g_pos3, g_normal, g_uv10},
        {g_pos4, g_normal, g_uv00},

        {g_pos5, g_normal, g_uv01},
        {g_pos6, g_normal, g_uv11},
        {g_pos7, g_normal, g_uv10},
        {g_pos8, g_normal, g_uv00},

        {g_pos9, g_normal, g_uv01},
        {g_pos10, g_normal, g_uv11},
        {g_pos11, g_normal, g_uv10},
        {g_pos12, g_normal, g_uv00}
    };
    
    std::vector<uint32_t> grassIndices = { 
        0, 1, 2, 2, 3, 0,  2, 1, 0, 0, 3, 2,
        4, 5, 6, 6, 7, 4,  6, 5, 4, 4, 7, 6,
        8, 9, 10, 10, 11, 8,  10, 9, 8, 8, 11, 10
    };

    // Create mesh
    grassMeshIndex = resourceManager.addMesh("GrassMesh", grassVertices, grassIndices);

    // Create material
    grassMaterialIndex = resourceManager.addMaterial("GrassMaterial",
        Material::fastTemplate(
            1.0f, 0.0f, 0.0f, 0.7f, // 0.7f discard threshold for transparency
            resourceManager.addTexture("GrassTexture", "Assets/Textures/Grass.png", TextureMode::ClampToEdge)
        )
    );
}

void Grass::generateGrassInstances(std::mt19937& generator) {
    windGrassInstances.clear();
    grassInstances.clear();

    // Find terrain height range for elevation-based coloring
    float minTerrainHeight = std::numeric_limits<float>::max();
    float maxTerrainHeight = std::numeric_limits<float>::lowest();
    for (int x = 0; x < config.worldSizeX; ++x) {
        for (int z = 0; z < config.worldSizeZ; ++z) {
            float height = heightMap[x][z] * heightScale;
            minTerrainHeight = std::min(minTerrainHeight, height);
            maxTerrainHeight = std::max(maxTerrainHeight, height);
        }
    }

    // Generate grass based on terrain characteristics
    for (int gridX = 0; gridX < config.worldSizeX - 1; ++gridX) {
        for (int gridZ = 0; gridZ < config.worldSizeZ - 1; ++gridZ) {

            // Variable density based on terrain
            std::uniform_real_distribution<float> rnd_density_mod(  1.0f - config.densityVariation, 
                                                                    1.0f + config.densityVariation);
            int baseGrassDensity = static_cast<int>(config.baseDensity * rnd_density_mod(generator));
            
            // Calculate average steepness for this grid cell
            float avgSteepness = 0.0f;
            int steepnessSamples = 0;
            for (int sx = 0; sx < 3; ++sx) {
                for (int sz = 0; sz < 3; ++sz) {
                    float sampleX = (gridX + sx * 0.5f) * terrainScale;
                    float sampleZ = (gridZ + sz * 0.5f) * terrainScale;
                    auto [_, sampleNormal] = getTerrainInfoAt(sampleX, sampleZ);
                    avgSteepness += (1.0f - sampleNormal.y);
                    steepnessSamples++;
                }
            }
            if (steepnessSamples > 0) {
                avgSteepness /= steepnessSamples;
            }
            
            // Add extra grass attempts based on steepness
            float steepnessMultiplier = 1.0f + (avgSteepness * config.steepnessMultiplier);
            int totalGrassAttempts = static_cast<int>(baseGrassDensity * steepnessMultiplier);
            
            for (int attempt = 0; attempt < totalGrassAttempts; ++attempt) {
                std::uniform_real_distribution<float> rnd_offset(config.offsetMin, config.offsetMax);
                std::uniform_real_distribution<float> rnd_rot(0.0f, 2.0f * glm::pi<float>());
                
                // Random position within the grid cell
                float worldX = (gridX + rnd_offset(generator)) * terrainScale;
                float worldZ = (gridZ + rnd_offset(generator)) * terrainScale;
                
                auto [terrainHeight, terrainNormal] = getTerrainInfoAt(worldX, worldZ);

                // For steeper areas, add random variation
                bool isExtraFromSteepness = attempt >= baseGrassDensity;
                if (isExtraFromSteepness) {
                    std::uniform_real_distribution<float> rnd_steep_chance(0.0f, 1.0f);
                    if (rnd_steep_chance(generator) > config.steepnessSpawnChance) continue;
                }
                
                // Calculate elevation factor (0 = lowest, 1 = highest)
                float elevationFactor = (terrainHeight - minTerrainHeight) / 
                                        std::max(0.1f, maxTerrainHeight - minTerrainHeight);
                
                // Grass height based on elevation
                float baseGrassHeight = 1.0f;
                if (elevationFactor < config.lowElevationThreshold) {
                    // Valleys - tall grass
                    std::uniform_real_distribution<float> rnd_height(config.valleyHeightMin, config.valleyHeightMax);
                    baseGrassHeight = rnd_height(generator);
                } else if (elevationFactor < config.highElevationThreshold) {
                    // Mid elevation - medium grass
                    std::uniform_real_distribution<float> rnd_height(config.midHeightMin, config.midHeightMax);
                    baseGrassHeight = rnd_height(generator);
                } else {
                    // High elevation - short grass
                    std::uniform_real_distribution<float> rnd_height(config.highHeightMin, config.highHeightMax);
                    baseGrassHeight = rnd_height(generator);
                    
                    // Make high elevation grass sparser
                    std::uniform_real_distribution<float> rnd_sparse(0.0f, 1.0f);
                    if (rnd_sparse(generator) > config.highElevationSparsity) continue;
                }
                
                // Color based on elevation and grass height
                glm::vec4 grassColor;
                if (elevationFactor < config.lowElevationThreshold) {
                    float heightFactor = (baseGrassHeight - config.valleyHeightMin) / 
                                        (config.valleyHeightMax - config.valleyHeightMin);
                    grassColor = glm::mix(config.richGreen, config.normalGreen, 
                                        std::clamp(heightFactor, 0.0f, 1.0f));
                } else if (elevationFactor < config.highElevationThreshold) {
                    grassColor = glm::mix(config.normalGreen, config.paleGreen, 
                                        (elevationFactor - config.lowElevationThreshold) / 
                                        (config.highElevationThreshold - config.lowElevationThreshold));
                } else {
                    grassColor = glm::mix(config.paleGreen, config.yellowishGreen, 
                                        (elevationFactor - config.highElevationThreshold) / 
                                        (1.0f - config.highElevationThreshold));
                }
                
                // Add color variation based on grass height
                float agingFactor = std::min(1.0f, baseGrassHeight / 1.5f);
                glm::vec4 youngColor = grassColor * config.colorBrightnessFactor;
                glm::vec4 oldColor = grassColor * config.colorDullnessFactor;
                grassColor = glm::mix(youngColor, oldColor, agingFactor);
                grassColor.a = 1.0f;
                
                // Create grass transform
                Transform grassTrform;
                grassTrform.pos = glm::vec3(worldX, terrainHeight, worldZ);
                grassTrform.scl = glm::vec3(1.0f, baseGrassHeight, 1.0f);
                grassTrform.rotateY(rnd_rot(generator));
                
                // Align grass to terrain normal
                glm::vec3 up(0.0f, 1.0f, 0.0f);
                if (glm::length(glm::cross(up, terrainNormal)) > 0.001f) {
                    glm::vec3 rotAxis = glm::normalize(glm::cross(up, terrainNormal));
                    float rotAngle = std::acos(glm::clamp(glm::dot(up, terrainNormal), -1.0f, 1.0f));
                    glm::quat tiltQuat = glm::angleAxis(rotAngle, rotAxis);
                    grassTrform.rot = tiltQuat * grassTrform.rot;
                }
                
                // Create wind-enabled grass instance
                float flexibility = 0.5f + (1.0f - terrainNormal.y) * 0.5f; // More flexible on slopes
                std::uniform_real_distribution<float> rnd_phase(0.0f, 6.28f);
                float phaseOffset = rnd_phase(generator);
                
                WindGrassInstance windGrassInstance(grassTrform.modelMatrix(), grassColor, 
                                                    baseGrassHeight, flexibility, phaseOffset);
                windGrassInstances.push_back(windGrassInstance);
                
                // Create regular instance for rendering
                ModelInstance grassInstance;
                grassInstance.modelMatrix() = grassTrform.modelMatrix();
                grassInstance.modelResourceIndex = grassModelIndex;
                grassInstance.multColor() = grassColor;
                grassInstances.push_back(grassInstance);
            }
        }
    }
}

void Grass::generateTerrainMesh(ResourceManager& resManager) {
    std::vector<Vertex> terrainVertices;
    std::vector<uint32_t> terrainIndices;

    // Generate vertices
    for (int x = 0; x < config.worldSizeX; ++x) {
        for (int z = 0; z < config.worldSizeZ; ++z) {
            float worldX = x * terrainScale;
            float worldZ = z * terrainScale;
            float worldY = heightMap[x][z] * heightScale;
            
            // Calculate smooth normal using neighboring heights
            glm::vec3 normal(0.0f, 1.0f, 0.0f);
            if (x > 0 && x < config.worldSizeX - 1 && z > 0 && z < config.worldSizeZ - 1) {
                float heightL = heightMap[x - 1][z] * heightScale;
                float heightR = heightMap[x + 1][z] * heightScale;
                float heightD = heightMap[x][z - 1] * heightScale;
                float heightU = heightMap[x][z + 1] * heightScale;
                
                float dX = (heightR - heightL) / (2.0f * terrainScale);
                float dZ = (heightU - heightD) / (2.0f * terrainScale);
                
                glm::vec3 tangentX(1.0f, dX, 0.0f);
                glm::vec3 tangentZ(0.0f, dZ, 1.0f);
                normal = glm::normalize(glm::cross(tangentZ, tangentX));
            }
            
            glm::vec2 uv(static_cast<float>(x) / (config.worldSizeX - 1), 
                        static_cast<float>(z) / (config.worldSizeZ - 1));

            terrainVertices.push_back({glm::vec3(worldX, worldY, worldZ), normal, uv});
        }
    }
    
    // Generate indices for triangles
    for (int x = 0; x < config.worldSizeX - 1; ++x) {
        for (int z = 0; z < config.worldSizeZ - 1; ++z) {
            uint32_t topLeft = x * config.worldSizeZ + z;
            uint32_t topRight = (x + 1) * config.worldSizeZ + z;
            uint32_t bottomLeft = x * config.worldSizeZ + (z + 1);
            uint32_t bottomRight = (x + 1) * config.worldSizeZ + (z + 1);
            
            terrainIndices.push_back(topLeft);
            terrainIndices.push_back(bottomLeft);
            terrainIndices.push_back(topRight);
            
            terrainIndices.push_back(topRight);
            terrainIndices.push_back(bottomLeft);
            terrainIndices.push_back(bottomRight);
        }
    }
    
    // Create terrain mesh and material
    terrainMeshIndex = resManager.addMesh("TerrainMesh", terrainVertices, terrainIndices);
    terrainMaterialIndex = resManager.addMaterial("TerrainMaterial",
        Material::fastTemplate(1.0f, 2.0f, 0.2f, 0.0f, 0));

    // Create terrain instance
    ModelInstance terrainInstance;
    terrainInstance.modelMatrix() = glm::mat4(1.0f);
    terrainInstance.modelResourceIndex = terrainModelIndex;
    terrainInstance.multColor() = glm::vec4(0.3411f, 0.5157f, 0.1549f, 1.0f);
    
    terrainInstances.push_back(terrainInstance);
}

std::pair<float, glm::vec3> Grass::getTerrainInfoAt(float worldX, float worldZ) const {
    float mapX_f = worldX / terrainScale;
    float mapZ_f = worldZ / terrainScale;
    
    int mapX = static_cast<int>(mapX_f);
    int mapZ = static_cast<int>(mapZ_f);
    
    if (mapX < 0 || mapX >= config.worldSizeX - 1 || mapZ < 0 || mapZ >= config.worldSizeZ - 1) {
        mapX = std::clamp(mapX, 0, config.worldSizeX - 1);
        mapZ = std::clamp(mapZ, 0, config.worldSizeZ - 1);
        return {heightMap[mapX][mapZ] * heightScale, glm::vec3(0.0f, 1.0f, 0.0f)};
    }
    
    float fx = mapX_f - mapX;
    float fz = mapZ_f - mapZ;
    
    float h00 = heightMap[mapX][mapZ] * heightScale;
    float h10 = heightMap[mapX + 1][mapZ] * heightScale;
    float h01 = heightMap[mapX][mapZ + 1] * heightScale;
    float h11 = heightMap[mapX + 1][mapZ + 1] * heightScale;
    
    float height = h00 * (1 - fx) * (1 - fz) + 
                  h10 * fx * (1 - fz) + 
                  h01 * (1 - fx) * fz + 
                  h11 * fx * fz;
    
    glm::vec3 normal(0.0f, 1.0f, 0.0f);
    if (mapX > 0 && mapX < config.worldSizeX - 1 && mapZ > 0 && mapZ < config.worldSizeZ - 1) {
        float heightL = heightMap[mapX - 1][mapZ] * heightScale;
        float heightR = heightMap[mapX + 1][mapZ] * heightScale;
        float heightD = heightMap[mapX][mapZ - 1] * heightScale;
        float heightU = heightMap[mapX][mapZ + 1] * heightScale;
        
        float dX = (heightR - heightL) / (2.0f * terrainScale);
        float dZ = (heightU - heightD) / (2.0f * terrainScale);
        
        glm::vec3 tangentX(1.0f, dX, 0.0f);
        glm::vec3 tangentZ(0.0f, dZ, 1.0f);
        normal = glm::normalize(glm::cross(tangentZ, tangentX));
    }
    
    return {height, normal};
}

void Grass::updateWindAnimation(float deltaTime) {
    if (!config.enableWind || windGrassInstances.empty()) {
        return;
    }
    
    windTime += deltaTime;
    
    // For now, use CPU-side wind animation that mirrors the compute shader
    // This provides the same visual effect without the GPU-CPU synchronization overhead
    updateGrassInstancesCPU(deltaTime);
    
    // TODO: Once we integrate compute shader results directly into rendering,
    // we can enable the GPU compute path:
    /*
    if (windComputePipeline != VK_NULL_HANDLE) {
        // Update wind uniform buffer
        if (windUniformBufferMapped) {
            WindUBO windUBO;
            glm::vec3 normalizedWindDir = glm::normalize(config.windDirection);
            windUBO.windDirection = glm::vec4(normalizedWindDir, config.windStrength);
            windUBO.windParams = glm::vec4(windTime, config.windFrequency, config.windTurbulence, config.windGustiness);
            windUBO.windWaves = glm::vec4(config.windWave1Freq, config.windWave2Freq, 
                                         config.windWave1Amp, config.windWave2Amp);
            
            memcpy(windUniformBufferMapped, &windUBO, sizeof(WindUBO));
        }
        
        // Dispatch compute shader
        // ... (GPU compute code)
    }
    */
}

void Grass::updateGrassInstancesCPU(float deltaTime) {
    glm::vec3 normalizedWindDir = glm::normalize(config.windDirection);

    // Apply wind animation to each grass instance
    for (size_t i = 0; i < windGrassInstances.size() && i < grassInstances.size(); ++i) {
        const auto& windData = windGrassInstances[i];
        float baseHeight = windData.windData.x;
        float flexibility = windData.windData.y;
        float phaseOffset = windData.windData.z;
        
        // Get base position (bottom of grass blade)
        glm::vec3 basePos = glm::vec3(windData.modelMatrix[3]);
        
        // Calculate wind effect (same as compute shader)
        float time = windTime + phaseOffset;
        
        // Primary wave
        float wave1 = sin(time * config.windWave1Freq) * config.windWave1Amp;
        // Secondary wave with different frequency
        float wave2 = sin(time * config.windWave2Freq) * config.windWave2Amp;
        
        // Noise-based turbulence (simplified)
        float noise = sin(basePos.x * 0.1f + time) * sin(basePos.z * 0.1f + time * 1.3f);
        float turbulence = noise * config.windTurbulence;
        
        // Gustiness (occasional stronger wind)
        float gust = sin(time * 0.5f) * config.windGustiness;
        
        // Combine all wind effects
        float windStrength = config.windStrength * (wave1 + wave2 + turbulence + gust);
        windStrength *= flexibility; // More flexible grass bends more
        
        // Calculate wind displacement
        glm::vec3 windDisplacement = normalizedWindDir * windStrength * baseHeight * 0.1f;
        
        // Create tilt transformation
        float tiltAmount = glm::length(windDisplacement) * 2.0f; // Amplify for visual effect
        tiltAmount = glm::clamp(tiltAmount, 0.0f, 1.0f);
        
        // Calculate tilt direction
        glm::vec3 tiltDirection = glm::normalize(windDisplacement);
        
        // Create tilt quaternion (tilt around axis perpendicular to wind direction)
        glm::vec3 up(0.0f, 1.0f, 0.0f);
        glm::vec3 tiltAxis = glm::cross(up, tiltDirection);
        if (glm::length(tiltAxis) > 0.001f) {
            tiltAxis = glm::normalize(tiltAxis);
            float tiltAngle = tiltAmount * glm::pi<float>() * 0.2f; // Max 36 degrees
            glm::quat tiltQuat = glm::angleAxis(tiltAngle, tiltAxis);
            
            // Extract original transform components
            glm::vec3 position = glm::vec3(windData.modelMatrix[3]);
            glm::vec3 scale = glm::vec3(
                glm::length(glm::vec3(windData.modelMatrix[0])),
                glm::length(glm::vec3(windData.modelMatrix[1])),
                glm::length(glm::vec3(windData.modelMatrix[2]))
            );
            
            // Get original rotation (simplified - assumes uniform scale)
            glm::mat3 rotMatrix = glm::mat3(windData.modelMatrix);
            rotMatrix[0] = glm::normalize(rotMatrix[0]);
            rotMatrix[1] = glm::normalize(rotMatrix[1]);
            rotMatrix[2] = glm::normalize(rotMatrix[2]);
            glm::quat originalRot = glm::quat_cast(rotMatrix);
            
            // Apply wind tilt to original rotation
            glm::quat finalRot = tiltQuat * originalRot;
            
            // Build final transform matrix
            glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);
            glm::mat4 rotationMatrix = glm::mat4_cast(finalRot);
            glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);
            
            auto& grassInstance = grassModelGroup.modelInstances[i];

            grassInstance.modelMatrix() = translationMatrix * rotationMatrix * scaleMatrix;

            // Add to bulk update list instead of individual queueUpdate()
            // updatedInstances.push_back(i);
        }
    }

    grassModelGroup.meshMapping.toUpdateIndices[grassMeshIndex] = grassInstanceUpdateQueue;
}