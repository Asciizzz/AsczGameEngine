#include "AzGame/Grass.hpp"

#include "Az3D/Az3D.hpp"
#include "AzVulk/AzVulk.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <execution>

using namespace AzGame;
using namespace Az3D;
using namespace AzVulk;

Grass::Grass(const GrassConfig& grassConfig) : config(grassConfig) {
    heightMap.resize(config.worldSizeX, std::vector<float>(config.worldSizeZ, 0.0f));
}

Grass::~Grass() {}

bool Grass::initialize(ResourceManager& resourceManager, const AzVulk::Device* vkDevice) {
    this->vkDevice = vkDevice;

    createGrassMesh90deg(resourceManager);

    std::mt19937 generator(std::random_device{}());
    generateHeightMap(generator);
    generateTerrainMesh(resourceManager);

    generateGrassInstances(generator);

    // grassInstanceGroup.init("GrassField", vkDevice);
    grassInstanceGroup.initVkDevice(vkDevice);
    grassInstanceGroup.meshIndex = grassMeshIndex;

    terrainInstanceGroup.initVkDevice(vkDevice);
    terrainInstanceGroup.meshIndex = terrainMeshIndex;

    setupComputeShaders();

    for (const auto& data : grassData3Ds) {
        grassInstanceGroup.addInstance(data);
    }
    for (const auto& data : terrainData3Ds) {
        terrainInstanceGroup.addInstance(data);
    }

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
                    float influence = config.influenceFactor * (1.0f + std::cos(normalizedDistance * glm::pi<float>()));
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

    std::vector<VertexStatic> grassVertices = {
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
    SharedPtr<MeshStatic> grassMesh = MakeShared<MeshStatic>(std::move(grassVertices), std::move(grassIndices));
    grassMeshIndex = resourceManager.addMesh("GrassMesh", grassMesh);

    // Create material
    Az3D::Material grassMaterial;
    grassMaterial.setShadingParams(true, 0, 0.5f, 0.9f);
    grassMaterial.setAlbedoTextureIndex(resourceManager.addTexture("GrassTexture", "Assets/Textures/Grass.png", Texture::ClampToEdge));

    grassMaterialIndex = resourceManager.addMaterial("GrassMaterial", grassMaterial);
}


void Grass::createGrassMesh90deg(Az3D::ResourceManager& resourceManager) {
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
    glm::vec3 g_pos5 = Transform::rotate(g_pos1, g_normal, glm::radians(90.0f));
    glm::vec3 g_pos6 = Transform::rotate(g_pos2, g_normal, glm::radians(90.0f));
    glm::vec3 g_pos7 = Transform::rotate(g_pos3, g_normal, glm::radians(90.0f));
    glm::vec3 g_pos8 = Transform::rotate(g_pos4, g_normal, glm::radians(90.0f));

    std::vector<VertexStatic> grassVertices = {
        {g_pos1, g_normal, g_uv01},
        {g_pos2, g_normal, g_uv11},
        {g_pos3, g_normal, g_uv10},
        {g_pos4, g_normal, g_uv00},

        {g_pos5, g_normal, g_uv01},
        {g_pos6, g_normal, g_uv11},
        {g_pos7, g_normal, g_uv10},
        {g_pos8, g_normal, g_uv00}
    };
    
    std::vector<uint32_t> grassIndices = { 
        0, 1, 2, 2, 3, 0,  2, 1, 0, 0, 3, 2,
        4, 5, 6, 6, 7, 4,  6, 5, 4, 4, 7, 6
    };

    // Create mesh
    SharedPtr<MeshStatic> grassMesh = MakeShared<MeshStatic>(std::move(grassVertices), std::move(grassIndices));
    grassMeshIndex = resourceManager.addMesh("GrassMesh", grassMesh);

    // Create material
    Az3D::Material grassMaterial;
    grassMaterial.setShadingParams(true, 0, 0.5f, 0.9f);
    grassMaterial.setAlbedoTextureIndex(resourceManager.addTexture("GrassTexture", "Assets/Textures/Grass.png", Texture::ClampToEdge));

    grassMaterialIndex = resourceManager.addMaterial("GrassMaterial", grassMaterial);
}

void Grass::generateGrassInstances(std::mt19937& generator) {
    windProps.clear();
    fixedMat4.clear();
    fixedColor.clear();
    grassMat4.clear();

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
                grassTrform.scl = glm::vec3(1.0f, baseGrassHeight * 1.2f, 1.0f);
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
                
                // Create regular instance for rendering
                Data3D grassInstance;
                grassInstance.modelMatrix = grassTrform.getMat4();
                grassInstance.multColor = grassColor;
                grassInstance.properties.x = grassMaterialIndex;

                // Store wind properties

                // SOA structure for compute shader
                windProps.push_back(glm::vec4(baseGrassHeight, flexibility, phaseOffset, 0.0f));
                fixedMat4.push_back(grassInstance.modelMatrix); 
                fixedColor.push_back(grassInstance.multColor);
                grassMat4.push_back(grassInstance.modelMatrix);

                grassData3Ds.push_back(grassInstance);
            }
        }
    }
}

void Grass::generateTerrainMesh(ResourceManager& resManager) {
    std::vector<VertexStatic> terrainVertices;
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

            terrainVertices.push_back(VertexStatic{glm::vec3(worldX, worldY, worldZ), normal, uv});
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
    SharedPtr<MeshStatic> terrainMesh = MakeShared<MeshStatic>(std::move(terrainVertices), std::move(terrainIndices));

    terrainMeshIndex = resManager.addMesh("TerrainMesh", terrainMesh, true);

    Material terrainMaterial;
    terrainMaterial.setShadingParams(true, 0, 0.2f, 0.0f);
    terrainMaterialIndex = resManager.addMaterial("TerrainMaterial", terrainMaterial);

    // Create terrain instance
    Data3D terrainData;
    terrainData.modelMatrix = glm::mat4(1.0f);
    terrainData.multColor = glm::vec4(0.3411f, 0.5157f, 0.1549f, 1.0f);
    terrainData.properties.x = terrainMaterialIndex;

    terrainData3Ds.push_back(terrainData);
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



void Grass::setupComputeShaders() {
    // Init
    fixedMat4Buffer.initVkDevice(vkDevice);
    windPropsBuffer.initVkDevice(vkDevice);
    grassMat4Buffer.initVkDevice(vkDevice);
    grassUniformBuffer.initVkDevice(vkDevice);

    ComputeTask::uploadDeviceStorageBuffer(fixedMat4Buffer, fixedMat4.data(), sizeof(glm::mat4) * fixedMat4.size());
    ComputeTask::uploadDeviceStorageBuffer(windPropsBuffer, windProps.data(), sizeof(glm::vec4) * windProps.size());
    ComputeTask::makeStorageBuffer(grassMat4Buffer, grassMat4.data(), sizeof(glm::mat4) * grassMat4.size());
    ComputeTask::makeUniformBuffer(grassUniformBuffer, &windTime, sizeof(float));

    grassComputeTask.init(vkDevice, "Shaders/Compute/grass.comp.spv");
    grassComputeTask.addStorageBuffer(fixedMat4Buffer, 0);
    grassComputeTask.addStorageBuffer(windPropsBuffer, 1);
    grassComputeTask.addStorageBuffer(grassMat4Buffer, 2);
    grassComputeTask.addUniformBuffer(grassUniformBuffer, 3);
    grassComputeTask.create();
}



void Grass::updateWindAnimation(float dTime, bool useGPU) {
    if (!config.enableWind) { return; }

    windTime += dTime * 0.5f;

    if (useGPU) updateGrassInstancesGPU();
    else        updateGrassInstancesCPU();
}

void Grass::updateGrassInstancesCPU() {
    glm::vec3 normalizedWindDir = glm::normalize(config.windDirection);

    std::vector<size_t> indices(grassMat4.size());
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
    [&](size_t i) {
        if (i >= windProps.size()) return;

        const glm::vec4& windData = windProps[i];
        float baseHeight = windData.x;
        float flexibility = windData.y;
        float phaseOffset = windData.z;

        // Get base position (bottom of grass blade)
        glm::vec3 basePos = glm::vec3(fixedMat4[i][3]);

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
            glm::vec3 position = glm::vec3(fixedMat4[i][3]);
            glm::vec3 scale = glm::vec3(
                glm::length(glm::vec3(fixedMat4[i][0])),
                glm::length(glm::vec3(fixedMat4[i][1])),
                glm::length(glm::vec3(fixedMat4[i][2]))
            );
            
            // Get original rotation (simplified - assumes uniform scale)
            glm::mat3 rotMatrix = glm::mat3(fixedMat4[i]);
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

            grassMat4[i] = translationMatrix * rotationMatrix * scaleMatrix;
        }


        // Set the updated grass matrix
        grassData3Ds[i].modelMatrix = grassMat4[i];
        grassData3Ds[i].multColor = fixedColor[i];
    });

    grassInstanceGroup.datas = grassData3Ds;
}

void Grass::updateGrassInstancesGPU() {
    // Mapped the time
    grassUniformBuffer.mappedData(&windTime);

    grassComputeTask.dispatchAsync(static_cast<uint32_t>(fixedMat4.size()), 128);

    glm::mat4* resultPtr = static_cast<glm::mat4*>(grassMat4Buffer.mapped);

    std::vector<size_t> indices(grassMat4.size());
    std::iota(indices.begin(), indices.end(), 0);

    // std::for_each(indices.begin(), indices.end(), [&](size_t i) {
    //     grassData3Ds[i].modelMatrix = resultPtr[i];
    // });

    grassInstanceGroup.datas = grassData3Ds;
}
