#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <random>
#include <limits>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <Az3D/ModelManager.hpp>
#include <AzVulk/ComputeTask.hpp>

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
    struct BufferData;
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
        float falloffRadius = 26.0f;
        float influenceFactor = 0.01f;
        
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
        glm::vec3 windDirection{1.0f, 1.0f, 0.5f};  // Wind direction (will be normalized)
        float windStrength = 2.0f;                  // Base wind strength
        float windFrequency = 1.5f;                 // Wave frequency
        float windTurbulence = 0.8f;                // Noise-based turbulence
        float windGustiness = 1.2f;                 // Occasional stronger gusts
        float windWave1Freq = 2.0f;                 // Primary wave frequency
        float windWave2Freq = 3.7f;                 // Secondary wave frequency  
        float windWave1Amp = 0.5f;                  // Primary wave amplitude
        float windWave2Amp = 0.3f;                  // Secondary wave amplitude
    };

    // Wind uniform buffer data
    struct WindUBO {
        glm::vec4 windDirection;  // xyz: direction, w: strength
        glm::vec4 windParams;     // x: time, y: frequency, z: turbulence, w: gustiness
        glm::vec4 windWaves;      // x: wave1 freq, y: wave2 freq, z: wave1 amp, w: wave2 amp
    };

    class Grass {
    public:
        using Data3D = Az3D::Model::Data3D;

        explicit Grass(const GrassConfig& config);
        ~Grass();

        // Delete copy constructor and assignment operator
        Grass(const Grass&) = delete;
        Grass& operator=(const Grass&) = delete;

        // Initialize the grass system
        bool initialize(Az3D::ResourceManager& resourceManager, const AzVulk::Device* vkDevice);

        // Wind animation functions (if enabled)
        void updateWindAnimation(float deltaTime, bool useGPU=true);
        void updateGrassInstancesCPU();
        void updateGrassInstancesGPU();

        // Configuration
        GrassConfig config;
        
        // Terrain data
        std::vector<std::vector<float>> heightMap;
        float terrainScale = 1.0f;
        float heightScale = 2.0f;
        
        // Grass instances
        std::vector<glm::vec4> windProps; // x: base height, y: flexibility, z: phase offset
        std::vector<glm::mat4> fixedMat4;
        std::vector<glm::vec4> fixedColor;
        std::vector<glm::mat4> grassMat4;
        std::vector<Data3D> grassData3Ds;

        // Grass buffer
        AzVulk::BufferData fixedMat4Buffer;
        AzVulk::BufferData windPropsBuffer;
        AzVulk::BufferData grassMat4Buffer;
        AzVulk::BufferData grassUniformBuffer;

        std::vector<Data3D> terrainData3Ds;

        // Resource indices
        size_t grassMeshIndex = 0;
        size_t grassMaterialIndex = 0;
        size_t grassModelHash = 0;

        size_t terrainMeshIndex = 0;
        size_t terrainMaterialIndex = 0;
        size_t terrainModelHash = 0;

        const AzVulk::Device* vkDevice = nullptr;

        // Model Group
        Az3D::ModelGroup grassFieldModelGroup;

        // Compute Task
        AzVulk::ComputeTask grassComputeTask;

        // Time tracking for wind animation
        float windTime = 0.0f;
        
        // Helper functions
        void generateHeightMap(std::mt19937& generator);
        void createGrassMesh(Az3D::ResourceManager& resManager);
        void createGrassMesh90deg(Az3D::ResourceManager& resManager);
        void generateGrassInstances(std::mt19937& generator);
        void generateTerrainMesh(Az3D::ResourceManager& resManager);
        std::pair<float, glm::vec3> getTerrainInfoAt(float worldX, float worldZ) const;

        void setupComputeShaders();
    };

} // namespace AzGame
