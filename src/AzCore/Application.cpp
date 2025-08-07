#include "AzCore/Application.hpp"

#include <iostream>
#include <random>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = false;
#endif

// You're welcome
using namespace AzVulk;
using namespace AzBeta;
using namespace AzCore;
using namespace Az3D;

Application::Application(const char* title, uint32_t width, uint32_t height)
    : appTitle(title), appWidth(width), appHeight(height) {

    windowManager = std::make_unique<AzCore::WindowManager>(title, width, height);
    fpsManager = std::make_unique<AzCore::FpsManager>();

    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    // 10km view distance for those distant horizons
    camera = std::make_unique<Camera>(glm::vec3(0.0f), 45.0f, 0.01f, 10000.0f);
    camera->setAspectRatio(aspectRatio);

    initVulkan();
}

Application::~Application() {
    cleanup();
}

void Application::run() {
    mainLoop();

    printf("Application exited successfully. See you next time! o/\n");
}

void Application::initVulkan() {
    auto extensions = windowManager->getRequiredVulkanExtensions();
    vulkanInstance = std::make_unique<Instance>(extensions, enableValidationLayers);
    createSurface();

    vulkanDevice = std::make_unique<Device>(vulkanInstance->instance, surface);
    msaaManager = std::make_unique<MSAAManager>(*vulkanDevice);
    swapChain = std::make_unique<SwapChain>(*vulkanDevice, surface, windowManager->window);

    opaquePipeline = std::make_unique<RasterPipeline>(
        vulkanDevice->device,
        swapChain->extent,
        swapChain->imageFormat,
        "Shaders/Rasterize/raster.vert.spv",
        "Shaders/Rasterize/raster.frag.spv",
        RasterPipelineConfig::createOpaqueConfig(msaaManager->msaaSamples)
    );

    // Create a separate transparent pipeline
    transparentPipeline = std::make_unique<RasterPipeline>(
        vulkanDevice->device,
        swapChain->extent,
        swapChain->imageFormat,
        "Shaders/Rasterize/raster.vert.spv",
        "Shaders/Rasterize/raster.frag.spv",
        RasterPipelineConfig::createTransparentConfig(msaaManager->msaaSamples)
    );

    shaderManager = std::make_unique<ShaderManager>(vulkanDevice->device);

    // Create command pool for graphics operations
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = vulkanDevice->queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(vulkanDevice->device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }

    // Initialize render targets and depth testing
    msaaManager->createColorResources(swapChain->extent.width, swapChain->extent.height, swapChain->imageFormat);
    depthManager = std::make_unique<DepthManager>(*vulkanDevice);
    depthManager->createDepthResources(swapChain->extent.width, swapChain->extent.height, msaaManager->msaaSamples);
    swapChain->createFramebuffers(opaquePipeline->renderPass, depthManager->depthImageView, msaaManager->colorImageView);

    buffer = std::make_unique<Buffer>(*vulkanDevice);
    buffer->createUniformBuffers(2);

    resourceManager = std::make_unique<ResourceManager>(*vulkanDevice, commandPool);
    renderSystem = std::make_unique<RenderSystem>();
    
    // Set up the render system
    renderSystem->setResourceManager(resourceManager.get());

    descriptorManager = std::make_unique<DescriptorManager>(*vulkanDevice, opaquePipeline->descriptorSetLayout);

    // Create convenient references to avoid arrow spam
    auto& resManager = *resourceManager;
    auto& texManager = *resManager.textureManager;
    auto& meshManager = *resManager.meshManager;
    auto& matManager = *resManager.materialManager;
    auto& rendSystem = *renderSystem;

// PLAYGROUND FROM HERE!

    // Load the global pallete texture that will be used for all platformer assets
    size_t globalMaterialIndex = resManager.addMaterial("GlobalPalette",
        Material::fastTemplate(
            1.0f, 2.0f, 0.2f, 0.0f, // No discard threshold since the texture is opaque anyway
            resManager.addTexture("GlobalPalette", "Assets/Platformer/Palette.png")
        )
    );
    
    // DEBUG: Check the material after adding it to the manager
    const auto& storedMaterial = *resManager.materialManager->materials[globalMaterialIndex];

    // Useful shorthand function for adding platformer meshes
    std::unordered_map<std::string, size_t> platformerMeshIndices;

    auto addPlatformerMesh = [&](std::string name, std::string path) {
        std::string fullName = "Platformer/" + name;
        std::string fullPath = "Assets/Platformer/" + path;

        platformerMeshIndices[name] = resManager.addMesh(fullName, fullPath, true);
    };

    // Useful shorthand for getting a model resource index
    auto getPlatformIndex = [&](const std::string& name) {
        return rendSystem.getModelResource("Platformer/" + name);
    };
    // Useful shorthand for placing models
    auto placePlatform = [&](const std::string& name, const Transform& transform, const glm::vec4& color = glm::vec4(1.0f)) {
        ModelInstance instance;
        instance.modelMatrix() = transform.modelMatrix();
        instance.multColor() = color;
        instance.modelResourceIndex = getPlatformIndex(name);

        worldInstances.push_back(instance);
    };

    struct NameAndPath {
        std::string name;
        std::string path;
    };

    std::vector<NameAndPath> platformerMeshes = {
        {"Ground_x2", "ground_grass_2.obj"},
        {"Ground_x4", "ground_grass_4.obj"},
        {"Ground_x8", "ground_grass_8.obj"},
        
        {"Tree_1", "Tree_1.obj"},
        {"Tree_2", "Tree_2.obj"},

        {"TrailCurve_1", "trail_dirt_curved_1.obj"},
        {"TrailCurve_2", "trail_dirt_curved_2.obj"},
        {"TrailEnd_1", "trail_dirt_end_1.obj"},
        {"TrailEnd_2", "trail_dirt_end_2.obj"},

        {"Fence_x1", "fence_1.obj"},
        {"Fence_x2", "fence_2.obj"},
        {"Fence_x4", "fence_4.obj"},

        {"Flower", "flower.obj"},

        {"Grass_1", "grass_blades_1.obj"},
        {"Grass_2", "grass_blades_2.obj"},
        {"Grass_3", "grass_blades_3.obj"},
        {"Grass_4", "grass_blades_4.obj"}
    };

    for (const auto& mesh : platformerMeshes) {
        addPlatformerMesh(mesh.name, mesh.path);
    }
    for (const auto& [name, meshIndex] : platformerMeshIndices) {
        size_t modelResourceIndex = rendSystem.addModelResource(
            "Platformer/" + name, meshIndex, globalMaterialIndex
        );
    }

    // Construct a simple world

    int world_size_x = 0;
    int world_size_z = 0;
    for (int x = 0; x < world_size_x; ++x) {
        for (int z = 0; z < world_size_z; ++z) {
            Transform trform;
            trform.pos = glm::vec3(
                static_cast<float>(x) * 8.0f + 4.0f,
                0.0f,
                static_cast<float>(z) * 8.0f + 4.0f
            );
            placePlatform("Ground_x8", trform);
        }
    }

    
    // Set up random number generation
    std::random_device rd;
    std::mt19937 gen(rd());
    
    int numTrees = 0;
    for (int i = 0; i < numTrees; ++i) {
        float max_x = static_cast<float>(world_size_x * 8) - 2.0f;
        float max_z = static_cast<float>(world_size_z * 8) - 2.0f;

        std::uniform_real_distribution<float> rnd_x(1.0f, max_x);
        std::uniform_real_distribution<float> rnd_z(1.0f, max_z);
        std::uniform_real_distribution<float> rnd_scl(0.5f, 1.4f);
        std::uniform_real_distribution<float> rnd_rot(0.0f, 2.0f * glm::pi<float>());

        Transform treeTrform;
        treeTrform.pos = glm::vec3(rnd_x(gen), 0.0f, rnd_z(gen));
        treeTrform.scale(rnd_scl(gen));
        treeTrform.rotateY(rnd_rot(gen));

        std::string treeName = "Tree_" + std::to_string(i % 2 + 1);

        placePlatform(treeName, treeTrform);
    }

    int numFlowers = 0;
    for (int i = 0; i < numFlowers; ++i) {
    
        float max_x = static_cast<float>(world_size_x * 8) - 2.0f;
        float max_z = static_cast<float>(world_size_z * 8) - 2.0f;

        std::uniform_real_distribution<float> rnd_x(1.0f, max_x);
        std::uniform_real_distribution<float> rnd_z(1.0f, max_z);
        std::uniform_real_distribution<float> rnd_scl(0.2f, 0.4f);
        std::uniform_real_distribution<float> rnd_rot(0.0f, 2.0f * glm::pi<float>());
        std::uniform_real_distribution<float> rnd_color(0.0f, 1.0f);

        Transform flowerTrform;
        flowerTrform.pos = glm::vec3(rnd_x(gen), 0.0f, rnd_z(gen));
        flowerTrform.scale(rnd_scl(gen));
        flowerTrform.rotateY(rnd_rot(gen));

        glm::vec4 flowerColor(rnd_color(gen), rnd_color(gen), rnd_color(gen), 1.0f);
        placePlatform("Flower", flowerTrform, flowerColor);
    }

    int numGrass = 0;
    glm::vec4 grassYoung = glm::vec4(1.5f, 1.5f, 0.0f, 1.0f);
    glm::vec4 grassOld = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    for (int i = 0; i < numGrass; ++i) {
        float max_x = static_cast<float>(world_size_x * 8);
        float max_z = static_cast<float>(world_size_z * 8);

        std::uniform_real_distribution<float> rnd_x(1.0f, max_x);
        std::uniform_real_distribution<float> rnd_z(1.0f, max_z);
        std::uniform_real_distribution<float> rnd_scl(0.2f, 0.4f);
        std::uniform_real_distribution<float> rnd_rot(0.0f, 2.0f * glm::pi<float>());
        std::uniform_int_distribution<int> rnd_grass_type(1, 4);

        Transform grassTrform;
        grassTrform.pos = glm::vec3(rnd_x(gen), 0.0f, rnd_z(gen));
        grassTrform.scale(glm::vec3(1.0f, rnd_scl(gen), 1.0f)); // Scale only on Y axis
        grassTrform.rotateY(rnd_rot(gen));

        // Colored grass based on scale (larger/older = slightly more green, smaller/younger = slightly more yellow)
        float greenFactor = (grassTrform.scl.y - 0.2f) * 5.0f; // Convert to [0.0, 1.0] range

        glm::vec4 grassColor = glm::mix(grassYoung, grassOld, greenFactor);

        std::string grassName = "Grass_" + std::to_string(rnd_grass_type(gen));
        placePlatform(grassName, grassTrform, grassColor);
    }


    // Create grasss for the grass texture

    glm::vec3 g_normal(0.0f, 1.0f, 0.0f);

    glm::vec2 g_uv00(0.0f, 0.0f);
    glm::vec2 g_uv10(1.0f, 0.0f);
    glm::vec2 g_uv11(1.0f, 1.0f);
    glm::vec2 g_uv01(0.0f, 1.0f);

    glm::vec3 g_pos1(-0.5f, 0.0f, 0.0f);
    glm::vec3 g_pos2(0.5f, 0.0f, 0.0f);
    glm::vec3 g_pos3(0.5f, 1.0f, 0.0f);
    glm::vec3 g_pos4(-0.5f, 1.0f, 0.0f);

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

    size_t grassMeshIndex = resManager.addMesh("GrassMesh", grassVertices, grassIndices);

    size_t grassMaterialIndex = resManager.addMaterial("grassMaterial",
        Material::fastTemplate(
            1.0f, 0.0f, 0.0f, 0.7f, // 0.7f discard threshold
            resManager.addTexture("GrassTexture", "Assets/Textures/Grass.png", TextureMode::ClampToEdge)
        )
    );

    size_t grassModelIndex = rendSystem.addModelResource("GrassModel", grassMeshIndex, grassMaterialIndex);

    // Grass configuration struct to organize all parameters
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
        int baseDensity = 8;                    // Increased base density further
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
    };
    
    GrassConfig grassConfig;

    // Generate height map first (moved this before grass placement)
    std::vector<std::vector<float>> heightMap;
    heightMap.resize(grassConfig.worldSizeX, std::vector<float>(grassConfig.worldSizeZ, 0.0f));
    
    // Generate height map using random nodes with smooth falloff
    std::uniform_int_distribution<int> rnd_x_node(0, grassConfig.worldSizeX - 1);
    std::uniform_int_distribution<int> rnd_z_node(0, grassConfig.worldSizeZ - 1);
    std::uniform_real_distribution<float> rnd_height(-grassConfig.lowVariance, grassConfig.heightVariance);
    
    for (int i = 0; i < grassConfig.numHeightNodes; ++i) {
        int centerX = rnd_x_node(gen);
        int centerZ = rnd_z_node(gen);
        float nodeHeight = rnd_height(gen);
        
        // Apply height to center node and surrounding area with smooth falloff
        int radiusInt = static_cast<int>(std::ceil(grassConfig.falloffRadius));
        
        for (int x = std::max(0, centerX - radiusInt); x < std::min(grassConfig.worldSizeX, centerX + radiusInt + 1); ++x) {
            for (int z = std::max(0, centerZ - radiusInt); z < std::min(grassConfig.worldSizeZ, centerZ + radiusInt + 1); ++z) {
                float distance = std::sqrt((x - centerX) * (x - centerX) + (z - centerZ) * (z - centerZ));
                
                if (distance <= grassConfig.falloffRadius) {
                    // Smooth falloff using cosine interpolation for natural curves
                    float normalizedDistance = distance / grassConfig.falloffRadius;
                    float influence = 0.5f * (1.0f + std::cos(normalizedDistance * glm::pi<float>()));
                    heightMap[x][z] += nodeHeight * influence;
                }
            }
        }
    }

    float terrainScale = 1.0f; // Scale between height map grid points
    float heightScale = 2.0f;  // Scale for height values

    // Helper function to get terrain height and normal at a world position
    auto getTerrainInfoAt = [&](float worldX, float worldZ) -> std::pair<float, glm::vec3> {
        // Convert world position to height map coordinates
        float mapX_f = worldX / terrainScale;
        float mapZ_f = worldZ / terrainScale;
        
        int mapX = static_cast<int>(mapX_f);
        int mapZ = static_cast<int>(mapZ_f);
        
        // Bounds check
        if (mapX < 0 || mapX >= grassConfig.worldSizeX - 1 || mapZ < 0 || mapZ >= grassConfig.worldSizeZ - 1) {
            // For edge cases, just use the nearest valid height
            mapX = std::clamp(mapX, 0, grassConfig.worldSizeX - 1);
            mapZ = std::clamp(mapZ, 0, grassConfig.worldSizeZ - 1);
            return {heightMap[mapX][mapZ] * heightScale, glm::vec3(0.0f, 1.0f, 0.0f)};
        }
        
        // Bilinear interpolation for smooth height
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
        
        // Calculate normal from surrounding heights
        glm::vec3 normal(0.0f, 1.0f, 0.0f);
        if (mapX > 0 && mapX < grassConfig.worldSizeX - 1 && mapZ > 0 && mapZ < grassConfig.worldSizeZ - 1) {
            float heightL = heightMap[mapX - 1][mapZ] * heightScale;
            float heightR = heightMap[mapX + 1][mapZ] * heightScale;
            float heightD = heightMap[mapX][mapZ - 1] * heightScale;
            float heightU = heightMap[mapX][mapZ + 1] * heightScale;
            
            // Calculate height differences
            float dX = (heightR - heightL) / (2.0f * terrainScale);
            float dZ = (heightU - heightD) / (2.0f * terrainScale);
            
            // Normal is the cross product of the tangent vectors
            glm::vec3 tangentX(1.0f, dX, 0.0f);
            glm::vec3 tangentZ(0.0f, dZ, 1.0f);
            normal = glm::normalize(glm::cross(tangentZ, tangentX));
        }
        
        return {height, normal};
    };

    // Enhanced grass placement system based on terrain characteristics
    std::vector<ModelInstance> grassInstances;
    
    // Find terrain height range for elevation-based coloring
    float minTerrainHeight = std::numeric_limits<float>::max();
    float maxTerrainHeight = std::numeric_limits<float>::lowest();
    for (int x = 0; x < grassConfig.worldSizeX; ++x) {
        for (int z = 0; z < grassConfig.worldSizeZ; ++z) {
            float height = heightMap[x][z] * heightScale;
            minTerrainHeight = std::min(minTerrainHeight, height);
            maxTerrainHeight = std::max(maxTerrainHeight, height);
        }
    }

    // Generate grass based on terrain characteristics with higher density
    for (int gridX = 0; gridX < grassConfig.worldSizeX - 1; ++gridX) {
        for (int gridZ = 0; gridZ < grassConfig.worldSizeZ - 1; ++gridZ) {

            // Variable density based on terrain - some areas are naturally more dense
            std::uniform_real_distribution<float> rnd_density_mod(1.0f - grassConfig.densityVariation, 1.0f + grassConfig.densityVariation);
            int baseGrassDensity = static_cast<int>(grassConfig.baseDensity * rnd_density_mod(gen));
            
            // Calculate average steepness for this grid cell to determine extra attempts
            float avgSteepness = 0.0f;
            int steepnessSamples = 0;
            for (int sx = 0; sx < 3; ++sx) {
                for (int sz = 0; sz < 3; ++sz) {
                    float sampleX = (gridX + sx * 0.5f) * terrainScale;
                    float sampleZ = (gridZ + sz * 0.5f) * terrainScale;
                    auto [_, sampleNormal] = getTerrainInfoAt(sampleX, sampleZ);
                    // Count all areas - no steepness limit
                    avgSteepness += (1.0f - sampleNormal.y);
                    steepnessSamples++;
                }
            }
            if (steepnessSamples > 0) {
                avgSteepness /= steepnessSamples;
            }
            
            // Add extra grass attempts based on average steepness of the area
            float steepnessMultiplier = 1.0f + (avgSteepness * grassConfig.steepnessMultiplier);
            int totalGrassAttempts = static_cast<int>(baseGrassDensity * steepnessMultiplier);
            
            for (int attempt = 0; attempt < totalGrassAttempts; ++attempt) {
                std::uniform_real_distribution<float> rnd_offset(grassConfig.offsetMin, grassConfig.offsetMax);
                std::uniform_real_distribution<float> rnd_rot(0.0f, 2.0f * glm::pi<float>());
                
                // Random position within the grid cell
                float worldX = (gridX + rnd_offset(gen)) * terrainScale;
                float worldZ = (gridZ + rnd_offset(gen)) * terrainScale;
                
                auto [terrainHeight, terrainNormal] = getTerrainInfoAt(worldX, worldZ);
                
                // Remove steep terrain limitation - grass grows everywhere now!
                // if (terrainNormal.y < grassConfig.minTerrainNormalY) continue;
                
                // For steeper areas, add slight random variation to avoid too much density
                bool isExtraFromSteepness = attempt >= baseGrassDensity;
                if (isExtraFromSteepness) {
                    std::uniform_real_distribution<float> rnd_steep_chance(0.0f, 1.0f);
                    if (rnd_steep_chance(gen) > grassConfig.steepnessSpawnChance) continue;
                }
                
                // Calculate elevation factor (0 = lowest, 1 = highest)
                float elevationFactor = (terrainHeight - minTerrainHeight) / 
                                        std::max(0.1f, maxTerrainHeight - minTerrainHeight);
                
                // Grass height based on elevation (lower = taller grass)
                float baseGrassHeight = 1.0f;
                if (elevationFactor < grassConfig.lowElevationThreshold) {
                    // Valleys - tall grass
                    std::uniform_real_distribution<float> rnd_height(grassConfig.valleyHeightMin, grassConfig.valleyHeightMax);
                    baseGrassHeight = rnd_height(gen);
                } else if (elevationFactor < grassConfig.highElevationThreshold) {
                    // Mid elevation - medium grass
                    std::uniform_real_distribution<float> rnd_height(grassConfig.midHeightMin, grassConfig.midHeightMax);
                    baseGrassHeight = rnd_height(gen);
                } else {
                    // High elevation - short, sparse grass
                    std::uniform_real_distribution<float> rnd_height(grassConfig.highHeightMin, grassConfig.highHeightMax);
                    baseGrassHeight = rnd_height(gen);
                    
                    // Make high elevation grass sparser
                    std::uniform_real_distribution<float> rnd_sparse(0.0f, 1.0f);
                    if (rnd_sparse(gen) > grassConfig.highElevationSparsity) continue;
                }
                
                // Color based on elevation and grass height
                glm::vec4 grassColor;
                if (elevationFactor < grassConfig.lowElevationThreshold) {
                    // Low elevation - rich green to normal green based on grass height
                    float heightFactor = (baseGrassHeight - grassConfig.valleyHeightMin) / (grassConfig.valleyHeightMax - grassConfig.valleyHeightMin);
                    grassColor = glm::mix(grassConfig.richGreen, grassConfig.normalGreen, std::clamp(heightFactor, 0.0f, 1.0f));
                } else if (elevationFactor < grassConfig.highElevationThreshold) {
                    // Mid elevation - normal green to pale green
                    grassColor = glm::mix(grassConfig.normalGreen, grassConfig.paleGreen, 
                                        (elevationFactor - grassConfig.lowElevationThreshold) / (grassConfig.highElevationThreshold - grassConfig.lowElevationThreshold));
                } else {
                    // High elevation - pale to yellowish
                    grassColor = glm::mix(grassConfig.paleGreen, grassConfig.yellowishGreen, 
                                        (elevationFactor - grassConfig.highElevationThreshold) / (1.0f - grassConfig.highElevationThreshold));
                }
                
                // Add slight color variation based on grass blade height
                float agingFactor = std::min(1.0f, baseGrassHeight / 1.5f);
                glm::vec4 youngColor = grassColor * grassConfig.colorBrightnessFactor;
                glm::vec4 oldColor = grassColor * grassConfig.colorDullnessFactor;
                grassColor = glm::mix(youngColor, oldColor, agingFactor);
                grassColor.a = 1.0f; // Ensure alpha is 1
                
                // Create grass transform
                Transform grassTrform;
                grassTrform.pos = glm::vec3(worldX, terrainHeight, worldZ);
                grassTrform.scl = glm::vec3(1.0f, baseGrassHeight, 1.0f);
                grassTrform.rotateY(rnd_rot(gen));
                
                // Align grass to terrain normal (full alignment, not just a slight tilt)
                glm::vec3 up(0.0f, 1.0f, 0.0f);
                
                // Calculate rotation to align grass with terrain normal
                if (glm::length(glm::cross(up, terrainNormal)) > 0.001f) {
                    glm::vec3 rotAxis = glm::normalize(glm::cross(up, terrainNormal));
                    float rotAngle = std::acos(glm::clamp(glm::dot(up, terrainNormal), -1.0f, 1.0f));
                    glm::quat tiltQuat = glm::angleAxis(rotAngle, rotAxis); // Full rotation, no reduction
                    grassTrform.rot = tiltQuat * grassTrform.rot;
                }
                
                // Create grass instance
                ModelInstance grassInstance;
                grassInstance.modelMatrix() = grassTrform.modelMatrix();
                grassInstance.modelResourceIndex = grassModelIndex;
                grassInstance.multColor() = grassColor;
                
                grassInstances.push_back(grassInstance);
            }
        }
    }

    // Generate terrain mesh from height map
    std::vector<Vertex> terrainVertices;
    std::vector<uint32_t> terrainIndices;
    
    // Generate vertices
    for (int x = 0; x < grassConfig.worldSizeX; ++x) {
        for (int z = 0; z < grassConfig.worldSizeZ; ++z) {
            float worldX = x * terrainScale;
            float worldZ = z * terrainScale;
            float worldY = heightMap[x][z] * heightScale;
            
            // Calculate smooth normal using neighboring heights
            glm::vec3 normal(0.0f, 1.0f, 0.0f);
            if (x > 0 && x < grassConfig.worldSizeX - 1 && z > 0 && z < grassConfig.worldSizeZ - 1) {
                float heightL = heightMap[x - 1][z] * heightScale;
                float heightR = heightMap[x + 1][z] * heightScale;
                float heightD = heightMap[x][z - 1] * heightScale;
                float heightU = heightMap[x][z + 1] * heightScale;
                
                // Calculate height differences
                float dX = (heightR - heightL) / (2.0f * terrainScale);
                float dZ = (heightU - heightD) / (2.0f * terrainScale);
                
                // Normal is the cross product of the tangent vectors
                glm::vec3 tangentX(1.0f, dX, 0.0f);
                glm::vec3 tangentZ(0.0f, dZ, 1.0f);
                normal = glm::normalize(glm::cross(tangentZ, tangentX)); // Note order for correct winding
            }
            
            // UV coordinates (0-1 range across the terrain)
            glm::vec2 uv(static_cast<float>(x) / (grassConfig.worldSizeX - 1), 
                        static_cast<float>(z) / (grassConfig.worldSizeZ - 1));

            terrainVertices.push_back({glm::vec3(worldX, worldY, worldZ), normal, uv});
        }
    }
    
    // Generate indices for triangles
    for (int x = 0; x < grassConfig.worldSizeX - 1; ++x) {
        for (int z = 0; z < grassConfig.worldSizeZ - 1; ++z) {
            uint32_t topLeft = x * grassConfig.worldSizeZ + z;
            uint32_t topRight = (x + 1) * grassConfig.worldSizeZ + z;
            uint32_t bottomLeft = x * grassConfig.worldSizeZ + (z + 1);
            uint32_t bottomRight = (x + 1) * grassConfig.worldSizeZ + (z + 1);
            
            // First triangle (top-left, bottom-left, top-right)
            terrainIndices.push_back(topLeft);
            terrainIndices.push_back(bottomLeft);
            terrainIndices.push_back(topRight);
            
            // Second triangle (top-right, bottom-left, bottom-right)
            terrainIndices.push_back(topRight);
            terrainIndices.push_back(bottomLeft);
            terrainIndices.push_back(bottomRight);
        }
    }
    
    // Create terrain mesh and material
    size_t terrainMeshIndex = resManager.addMesh("TerrainMesh", terrainVertices, terrainIndices);
    
    size_t terrainMaterialIndex = resManager.addMaterial("TerrainMaterial",
        Material::fastTemplate(
            1.0f, 2.0f, 0.2f, 0.0f, 0 // No discard threshold/No texture
        )
    );
    
    size_t terrainModelIndex = rendSystem.addModelResource("TerrainModel", terrainMeshIndex, 0);
    
    // Add terrain to world instances
    ModelInstance terrainInstance;
    terrainInstance.modelMatrix() = glm::mat4(1.0f); // Identity matrix - no transformation
    terrainInstance.modelResourceIndex = terrainModelIndex;
    terrainInstance.multColor() = glm::vec4(0.5411f, 0.8157f, 0.2549f, 1.0f); // Soft green color

    worldInstances.push_back(terrainInstance);
    
    // Add all grass instances to world instances
    worldInstances.insert(worldInstances.end(), grassInstances.begin(), grassInstances.end());

    rendSystem.addInstances(worldInstances);

    // Printing every Mesh - Material - Texture - Model information
    const char* COLORS[] = {
    "\x1b[31m", // Red
    "\x1b[32m", // Green
    "\x1b[33m", // Yellow
    "\x1b[34m", // Blue
    "\x1b[35m", // Magenta
    "\x1b[36m"  // Cyan
    };
    const char* WHITE = "\x1b[37m";
    const int NUM_COLORS = sizeof(COLORS) / sizeof(COLORS[0]);

    printf("%sLoaded Resources:\n> Meshes:\n", WHITE);
    for (const auto& [name, index] : resManager.meshNameToIndex)
        printf("%s   Idx %zu: %s\n", COLORS[index % NUM_COLORS], index, name.c_str());
    printf("%s> Textures:\n", WHITE);
    for (const auto& [name, index] : resManager.textureNameToIndex) {
        const auto& texture = texManager.textures[index];
        const char* color = COLORS[index % NUM_COLORS];

        printf("%s   Idx %zu: %s %s-> %sPATH: %s\n", color, index, name.c_str(), WHITE, color, texture.path.c_str());
    }
    printf("%s> Materials:\n", WHITE);
    for (const auto& [name, index] : resManager.materialNameToIndex) {
        const auto& material = *resManager.materialManager->materials[index];
        const char* color = COLORS[index % NUM_COLORS];

        if (material.diffTxtr > 0) {
            const auto& texture = resManager.textureManager->textures[material.diffTxtr];
            const char* diffColor = COLORS[material.diffTxtr % NUM_COLORS];

            printf("%s   Idx %zu: %s %s-> %sDIFF: Idx %zu\n",
                color, index, name.c_str(), WHITE,
                diffColor, material.diffTxtr
            );
        } else {
            printf("%s   Idx %zu: %s %s(TX none)\n",
                color, index, name.c_str(), WHITE);
        }
    }
    printf("%s> Model:\n", WHITE);
    for (const auto& [name, index] : renderSystem->modelResourceNameToIndex) {
        const auto& modelResource = renderSystem->modelResources[index];
        size_t meshIndex = modelResource.meshIndex;
        size_t materialIndex = modelResource.materialIndex;

        const char* color = COLORS[index % NUM_COLORS];
        const char* meshColor = COLORS[meshIndex % NUM_COLORS];
        const char* materialColor = COLORS[materialIndex % NUM_COLORS];

        printf("%s   Idx %zu %s(%sMS: %zu%s, %sMT: %zu%s): %s%s\n",
            color, index, WHITE,
            meshColor, meshIndex, WHITE,
            materialColor, materialIndex, WHITE,
            color, name.c_str()
        );
    }
    printf("%s", WHITE);


// PLAYGROUND END HERE 

    auto& bufferRef = *buffer;
    auto& descManager = *descriptorManager;

    // Create material uniform buffers
    std::vector<Material> materialVector;
    for (const auto& matPtr : matManager.materials) {
        materialVector.push_back(*matPtr);
    }
    bufferRef.createMaterialUniformBuffers(materialVector);

    // Create descriptor sets for materials
    descManager.createDescriptorPool(2, matManager.materials.size());

    for (size_t i = 0; i < matManager.materials.size(); ++i) {
        VkBuffer materialUniformBuffer = bufferRef.getMaterialUniformBuffer(i);
        
        // Get the correct texture index from the material
        size_t textureIndex = matManager.materials[i]->diffTxtr;

        // Safety check to prevent out of bounds access
        if (textureIndex >= texManager.textures.size())
            throw std::runtime_error(
                "Material references texture index " + std::to_string(textureIndex) + 
                " but only " + std::to_string(texManager.textures.size()) + " textures are loaded!"
            );
        
        descManager.createDescriptorSetsForMaterialWithUBO(
            bufferRef.uniformBuffers, sizeof(GlobalUBO), 
            &texManager.textures[textureIndex], materialUniformBuffer, i
        );
    }

    // Load meshes into GPU buffer
    for (size_t i = 0; i < meshManager.meshes.size(); ++i) {
        bufferRef.loadMeshToBuffer(*meshManager.meshes[i]);
    }

    // Final Renderer setup with ResourceManager
    renderer = std::make_unique<Renderer>(*vulkanDevice, *swapChain, *buffer, 
                                        *descriptorManager, *resourceManager);
}

void Application::createSurface() {
    if (!SDL_Vulkan_CreateSurface(windowManager->window, vulkanInstance->instance, &surface)) {
        throw std::runtime_error("failed to create window surface!");
    }
}

void Application::mainLoop() {
    SDL_SetRelativeMouseMode(SDL_TRUE); // Start with mouse locked

    // Create references to avoid arrow spam
    auto& winManager = *windowManager;
    auto& fpsRef = *fpsManager;
    auto& camRef = *camera;

    auto& rendererRef = *renderer;
    auto& deviceRef = *vulkanDevice;
    auto& bufferRef = *buffer;

    auto& resManager = *resourceManager;
    auto& meshManager = *resManager.meshManager;
    auto& texManager = *resManager.textureManager;
    auto& matManager = *resManager.materialManager;

    auto& rendSys = *renderSystem;

    while (!winManager.shouldCloseFlag) {
        // Update FPS manager for timing
        fpsRef.update();
        winManager.pollEvents();

        float dTime = fpsRef.deltaTime;

        static float cam_dist = 1.5f;
        static glm::vec3 camPos = camRef.pos;
        static bool mouseLocked = true;

        // Check if window was resized or renderer needs to be updated
        if (winManager.resizedFlag || rendererRef.framebufferResized) {
            winManager.resizedFlag = false;
            rendererRef.framebufferResized = false;

            vkDeviceWaitIdle(deviceRef.device);

            int newWidth, newHeight;
            SDL_GetWindowSize(winManager.window, &newWidth, &newHeight);

            // Reset like literally everything
            camRef.updateAspectRatio(newWidth, newHeight);
            msaaManager->createColorResources(newWidth, newHeight, swapChain->imageFormat);
            depthManager->createDepthResources(newWidth, newHeight, msaaManager->msaaSamples);
            swapChain->recreate(winManager.window, opaquePipeline->renderPass, depthManager->depthImageView, msaaManager->colorImageView);
            opaquePipeline->recreate(swapChain->extent, swapChain->imageFormat, RasterPipelineConfig::createOpaqueConfig(msaaManager->msaaSamples));
            transparentPipeline->recreate(swapChain->extent, swapChain->imageFormat, RasterPipelineConfig::createTransparentConfig(msaaManager->msaaSamples));
        }

        const Uint8* k_state = SDL_GetKeyboardState(nullptr);
        if (k_state[SDL_SCANCODE_ESCAPE]) {
            winManager.shouldCloseFlag = true;
            break;
        }

        // Toggle fullscreen with F11 key
        static bool f11Pressed = false;
        if (k_state[SDL_SCANCODE_F11] && !f11Pressed) {
            // Get current window flags
            Uint32 flags = SDL_GetWindowFlags(winManager.window);
            
            if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
                SDL_SetWindowFullscreen(winManager.window, 0);
            } else {
                SDL_SetWindowFullscreen(winManager.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
            f11Pressed = true;
        } else if (!k_state[SDL_SCANCODE_F11]) {
            f11Pressed = false;
        }
        
        // Toggle mouse lock with F1 key
        static bool f1Pressed = false;
        if (k_state[SDL_SCANCODE_F1] && !f1Pressed) {
            mouseLocked = !mouseLocked;
            if (mouseLocked) {
                SDL_SetRelativeMouseMode(SDL_TRUE);
                SDL_WarpMouseInWindow(winManager.window, 0, 0);
            } else {
                SDL_SetRelativeMouseMode(SDL_FALSE);
            }
            f1Pressed = true;
        } else if (!k_state[SDL_SCANCODE_F1]) {
            f1Pressed = false;
        }

        // Handle mouse look when locked
        if (mouseLocked) {
            int mouseX, mouseY;
            SDL_GetRelativeMouseState(&mouseX, &mouseY);

            float sensitivity = 0.02f;
            float yawDelta = mouseX * sensitivity;
            float pitchDelta = -mouseY * sensitivity;

            camRef.rotate(pitchDelta, yawDelta, 0.0f);
        }
    
// ======== PLAYGROUND HERE! ========


        // Camera movement controls
        bool fast = k_state[SDL_SCANCODE_LSHIFT] && !k_state[SDL_SCANCODE_LCTRL];
        bool slow = k_state[SDL_SCANCODE_LCTRL] && !k_state[SDL_SCANCODE_LSHIFT];
        float p_speed = (fast ? 26.0f : (slow ? 0.5f : 8.0f)) * dTime;

        if (k_state[SDL_SCANCODE_W]) camPos += camRef.forward * p_speed;
        if (k_state[SDL_SCANCODE_S]) camPos -= camRef.forward * p_speed;
        if (k_state[SDL_SCANCODE_A]) camPos -= camRef.right * p_speed;
        if (k_state[SDL_SCANCODE_D]) camPos += camRef.right * p_speed;

        camRef.pos = camPos;

        // Clear and populate the render system for this frame
        rendSys.clearInstances();
        rendSys.addInstances(worldInstances);

// =================================

        // Use the new render system instead of combining model vectors
        rendererRef.drawScene(*opaquePipeline, *transparentPipeline, camRef, rendSys);

        // On-screen FPS display (toggleable with F2) - using window title for now
        static auto lastFpsOutput = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsOutput).count() >= 500) {
            // Update FPS text every 500ms for smooth display
            std::string fpsText = "AsczGame | FPS: " + std::to_string(static_cast<int>(fpsRef.currentFPS)) +
                                    " | Avg: " + std::to_string(static_cast<int>(fpsRef.getAverageFPS())) +
                                    " | " + std::to_string(static_cast<int>(fpsRef.frameTimeMs * 10) / 10.0f) + "ms" +
                                    " | Pos: "+ std::to_string(camRef.pos.x) + ", " +
                                                std::to_string(camRef.pos.y) + ", " +
                                                std::to_string(camRef.pos.z);
            SDL_SetWindowTitle(winManager.window, fpsText.c_str());
            lastFpsOutput = now;
        }
    }

    vkDeviceWaitIdle(deviceRef.device);
}

void Application::cleanup() {
    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(vulkanDevice->device, commandPool, nullptr);
    }
    
    if (surface != VK_NULL_HANDLE && vulkanInstance) {
        vkDestroySurfaceKHR(vulkanInstance->instance, surface, nullptr);
    }
}