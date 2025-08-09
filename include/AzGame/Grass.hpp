#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <vector>
#include <random>
#include <memory>
#include <limits>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <Az3D/ModelManager.hpp>

// Forward declarations
namespace Az3D {
    struct ModelGroup;
    struct ResourceManager;
    struct MeshManager;

    struct Transform;
    struct Vertex;
}

namespace AzVulk {
    class Device;
}

namespace AzGame {

    struct GrassConfig {
        // World dimensions
        int worldSizeX = 64;
        int worldSizeZ = 64;
        
        // Terrain generation
        int numHeightNodes = 180;
        float heightVariance = 1.2f;
        float lowVariance = 0.4f;
        float falloffRadius = 6.0f;
        
        // Grass density and distribution
        int baseDensity = 8;                    // Base grass attempts per grid cell
        float densityVariation = 0.6f;         // Range: baseDensity * (1.0 ± densityVariation)
        float steepnessMultiplier = 4.0f;       // Up to 5x density on steep slopes
        float steepnessSpawnChance = 0.8f;      // 80% chance for extra steep grass
        
        // Grass placement
        float offsetMin = 0.0f;                 // Wider spread to avoid clustering
        float offsetMax = 1.0f;
        
        // Grass height ranges by elevation
        float valleyHeightMin = 1.2f;
        float valleyHeightMax = 1.8f;
        float midHeightMin = 0.8f;
        float midHeightMax = 1.4f;
        float highHeightMin = 0.3f;
        float highHeightMax = 0.8f;
        
        // Elevation thresholds
        float lowElevationThreshold = 0.3f;
        float highElevationThreshold = 0.7f;
        
        // Grass sparsity
        float highElevationSparsity = 1.0f;
        
        // Color definitions
        glm::vec4 richGreen{0.3f, 0.8f, 0.3f, 1.0f};      // Low elevation, lush
        glm::vec4 normalGreen{0.5f, 0.7f, 0.4f, 1.0f};    // Mid elevation, healthy
        glm::vec4 paleGreen{0.7f, 0.8f, 0.5f, 1.0f};      // High elevation, pale
        glm::vec4 yellowishGreen{0.8f, 0.8f, 0.4f, 1.0f}; // Very high/old grass
        
        // Color variation
        float colorBrightnessFactor = 1.2f;
        float colorDullnessFactor = 0.8f;
        
        // Wind animation parameters
        bool enableWind = true;
        glm::vec3 windDirection{1.0f, 0.0f, 0.5f};  // Wind direction (will be normalized)
        float windStrength = 2.0f;                  // Base wind strength
        float windFrequency = 1.5f;                 // Wave frequency
        float windTurbulence = 0.8f;                // Noise-based turbulence
        float windGustiness = 1.2f;                 // Occasional stronger gusts
        float windWave1Freq = 2.0f;                 // Primary wave frequency
        float windWave2Freq = 3.7f;                 // Secondary wave frequency  
        float windWave1Amp = 0.5f;                  // Primary wave amplitude
        float windWave2Amp = 0.3f;                  // Secondary wave amplitude
    };

    // Wind-enabled grass instance structure for compute shader
    struct WindGrassInstance {
        glm::mat4 modelMatrix;
        glm::vec4 color;
        glm::vec4 windData; // x: base height, y: flexibility, z: phase offset, w: unused
        
        WindGrassInstance() = default;
        WindGrassInstance(  const glm::mat4& transform, const glm::vec4& instanceColor, 
                            float baseHeight, float flexibility, float phaseOffset);
    };

    // Wind uniform buffer data
    struct WindUBO {
        glm::vec4 windDirection;  // xyz: direction, w: strength
        glm::vec4 windParams;     // x: time, y: frequency, z: turbulence, w: gustiness
        glm::vec4 windWaves;      // x: wave1 freq, y: wave2 freq, z: wave1 amp, w: wave2 amp
    };

    class Grass {
    public:
        explicit Grass(const GrassConfig& config);
        ~Grass();

        // Delete copy constructor and assignment operator
        Grass(const Grass&) = delete;
        Grass& operator=(const Grass&) = delete;

        // Initialize the grass system
        bool initialize(Az3D::ResourceManager& resourceManager);

        // Wind animation functions (if enabled)
        void updateWindAnimation(float deltaTime);
        void updateGrassInstancesCPU(float deltaTime);


        // Configuration
        GrassConfig config;
        
        // Terrain data
        std::vector<std::vector<float>> heightMap;
        float terrainScale = 1.0f;
        float heightScale = 2.0f;
        
        // Grass instances
        std::vector<WindGrassInstance> windGrassInstances;
        std::vector<Az3D::ModelInstance> grassInstances;
        std::vector<Az3D::ModelInstance> terrainInstances;
        
        // Resource indices
        size_t grassMeshIndex = 0;
        size_t grassMaterialIndex = 0;
        size_t grassModelIndex = 0;
        size_t terrainMeshIndex = 0;
        size_t terrainMaterialIndex = 0;
        size_t terrainModelIndex = 0;

        // Model Group
        Az3D::ModelGroup grassModelGroup;
        Az3D::ModelGroup terrainModelGroup;

        // Time tracking for wind animation
        float windTime = 0.0f;
        
        // Helper functions
        void generateHeightMap(std::mt19937& generator);
        void createGrassMesh(Az3D::ResourceManager& resManager);
        void generateGrassInstances(std::mt19937& generator);
        void generateTerrainMesh(Az3D::ResourceManager& resManager);
        std::pair<float, glm::vec3> getTerrainInfoAt(float worldX, float worldZ) const;
    };

} // namespace AzGame
