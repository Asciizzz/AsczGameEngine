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

    // Create shared render pass for forward rendering
    auto renderPassConfig = RenderPassConfig::createForwardRenderingConfig(
        swapChain->imageFormat, msaaManager->msaaSamples
    );
    mainRenderPass = std::make_unique<RenderPass>(vulkanDevice->device, renderPassConfig);

    // Create descriptor manager first and get the standard layout
    descriptorManager = std::make_unique<DescriptorManager>(*vulkanDevice);
    VkDescriptorSetLayout standardLayout = descriptorManager->createStandardRasterLayout();

    opaquePipeline = std::make_unique<RasterPipeline>(
        vulkanDevice->device,
        mainRenderPass->renderPass,
        standardLayout,
        "Shaders/Rasterize/raster.vert.spv",
        "Shaders/Rasterize/raster.frag.spv",
        RasterPipelineConfig::createOpaqueConfig(msaaManager->msaaSamples)
    );

    // Create a separate transparent pipeline (uses same render pass and layout)
    transparentPipeline = std::make_unique<RasterPipeline>(
        vulkanDevice->device,
        mainRenderPass->renderPass,
        standardLayout,
        "Shaders/Rasterize/raster.vert.spv",
        "Shaders/Rasterize/raster.frag.spv",
        RasterPipelineConfig::createTransparentConfig(msaaManager->msaaSamples)
    );

    // Create sky pipeline (renders first as background)
    skyPipeline = std::make_unique<RasterPipeline>(
        vulkanDevice->device,
        mainRenderPass->renderPass,
        standardLayout,
        "Shaders/Sky/sky.vert.spv",
        "Shaders/Sky/sky.frag.spv",
        RasterPipelineConfig::createSkyConfig(msaaManager->msaaSamples)
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
    swapChain->createFramebuffers(mainRenderPass->renderPass, depthManager->depthImageView, msaaManager->colorImageView);

    buffer = std::make_unique<Buffer>(*vulkanDevice);
    buffer->createUniformBuffers(2);

    resourceManager = std::make_unique<ResourceManager>(*vulkanDevice, commandPool);
    modelManager = std::make_unique<ModelManager>();

    // Create convenient references to avoid arrow spam
    auto& resManager = *resourceManager;
    auto& texManager = *resManager.textureManager;
    auto& meshManager = *resManager.meshManager;
    auto& matManager = *resManager.materialManager;
    auto& mdlManager = *modelManager;

// PLAYGROUND FROM HERE!

    // Load the global pallete texture that will be used for all platformer assets
    size_t globalMaterialIndex = resManager.addMaterial("GlobalPalette",
        Material::fastTemplate(
            1.0f, 2.0f, 0.2f, 0.0f, // No discard threshold since the texture is opaque anyway
            resManager.addTexture("GlobalPalette", "Assets/Platformer/Palette.png")
        )
    );


    // Useful shorthand function for adding platformer meshes
    std::unordered_map<std::string, size_t> platformerMeshIndices;

    auto addPlatformerMesh = [&](std::string name, std::string path) {
        std::string fullName = "Platformer/" + name;
        std::string fullPath = "Assets/Platformer/" + path;

        platformerMeshIndices[name] = resManager.addMesh(fullName, fullPath, true);
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

        {"Flower", "Icosphere.obj"},

        {"Grass_1", "grass_blades_1.obj"},
        {"Grass_2", "grass_blades_2.obj"},
        {"Grass_3", "grass_blades_3.obj"},
        {"Grass_4", "grass_blades_4.obj"}
    };

    for (const auto& mesh : platformerMeshes) {
        addPlatformerMesh(mesh.name, mesh.path);
    }
    for (const auto& [name, meshIndex] : platformerMeshIndices) {
        size_t modelResourceIndex = worldGroup.addModelResource(
            "Platformer/" + name, meshIndex, globalMaterialIndex
        );
    }

    // Construct a simple world

    // Useful shorthand for getting a model resource index
    auto getPlatformIndex = [&](const std::string& name) {
        return worldGroup.getModelResourceIndex("Platformer/" + name);
    };
    // Useful shorthand for placing models
    auto placePlatform = [&](const std::string& name, const Transform& transform, const glm::vec4& color = glm::vec4(1.0f)) {
        ModelInstance instance;
        instance.modelMatrix() = transform.modelMatrix();
        instance.multColor() = color;
        instance.modelResourceIndex = getPlatformIndex(name);

        worldGroup.addInstance(instance);
    };

    int world_size_x = 0;
    int world_size_z = 0;

    float ground_step_x = 2.0f;
    float ground_step_z = 2.0f;

    float half_ground_step_x = ground_step_x * 0.5f;
    float half_ground_step_z = ground_step_z * 0.5f;

    for (int x = 0; x < world_size_x; ++x) {
        for (int z = 0; z < world_size_z; ++z) {
            Transform trform;
            trform.pos = glm::vec3(
                static_cast<float>(x) * ground_step_x + half_ground_step_x,
                0.0f,
                static_cast<float>(z) * ground_step_z + half_ground_step_z
            );
            placePlatform("Flower", trform);
        }
    }


    // Set up random number generation
    std::random_device rd;
    std::mt19937 gen(rd());
    
    int numTrees = 0;
    for (int i = 0; i < numTrees; ++i) {
        float max_x = static_cast<float>(world_size_x * ground_step_x) - 2.0f;
        float max_z = static_cast<float>(world_size_z * ground_step_z) - 2.0f;

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
    
        float max_x = static_cast<float>(world_size_x * ground_step_x) - 2.0f;
        float max_z = static_cast<float>(world_size_z * ground_step_z) - 2.0f;

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
        float max_x = static_cast<float>(world_size_x * ground_step_x);
        float max_z = static_cast<float>(world_size_z * ground_step_z);

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

    worldGroup.buildMeshMapping();
    mdlManager.addGroup("World", worldGroup);


    // Set up advanced grass system with terrain generation
    AzGame::GrassConfig grassConfig;
    grassConfig.worldSizeX = 80;
    grassConfig.worldSizeZ = 80;
    grassConfig.baseDensity = 12;
    grassConfig.heightVariance = 3.9f;
    grassConfig.lowVariance = 0.1f;
    grassConfig.numHeightNodes = 150;
    grassConfig.enableWind = true;
    
    // Initialize grass system
    grassSystem = std::make_unique<AzGame::Grass>(grassConfig);
    if (!grassSystem->initialize(*resourceManager)) {
        throw std::runtime_error("Failed to initialize grass system!");
    }

    // Add only terrain to world instances (grass will be added dynamically each frame)

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

    printf("%s> [%s] Model Resources:\n", WHITE, worldGroup.name.c_str());
    for (const auto& [name, index] : worldGroup.modelResourceNameToIndex) {
        const auto& modelResource = worldGroup.modelResources[index];
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
        
        descManager.createDescriptorSets(
            bufferRef.uniformBuffers, sizeof(GlobalUBO), 
            &texManager.textures[textureIndex], materialUniformBuffer, i,
            depthManager->depthSamplerView, depthManager->depthSampler
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

    auto& mdlManager = *modelManager;

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
            
            // Recreate render pass with new settings
            auto newRenderPassConfig = RenderPassConfig::createForwardRenderingConfig(
                swapChain->imageFormat, msaaManager->msaaSamples);
            mainRenderPass->recreate(newRenderPassConfig);
            
            swapChain->recreate(winManager.window, mainRenderPass->renderPass, depthManager->depthImageView, msaaManager->colorImageView);
            opaquePipeline->recreate(mainRenderPass->renderPass, RasterPipelineConfig::createOpaqueConfig(msaaManager->msaaSamples));
            transparentPipeline->recreate(mainRenderPass->renderPass, RasterPipelineConfig::createTransparentConfig(msaaManager->msaaSamples));
            skyPipeline->recreate(mainRenderPass->renderPass, RasterPipelineConfig::createSkyConfig(msaaManager->msaaSamples));
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

        // Update grass wind animation
        static bool hold_y = false;
        static bool enable_wind = false;
        if (k_state[SDL_SCANCODE_Y] && !hold_y) {
            enable_wind = !enable_wind;
            hold_y = true;
        } else if (!k_state[SDL_SCANCODE_Y]) {
            hold_y = false;
        }

        if (grassSystem && enable_wind) {
            grassSystem->updateWindAnimation(dTime);
        }

// =================================

        // Use the new explicit rendering interface
        uint32_t imageIndex = rendererRef.beginFrame(*opaquePipeline, camRef);
        if (imageIndex != UINT32_MAX) {
            // First: render sky background with dedicated pipeline
            rendererRef.drawSky(*skyPipeline);
            
            // Second: render opaque objects
            auto& worldGroup = mdlManager.groups["World"];
            rendererRef.drawScene(*opaquePipeline, worldGroup);

            auto& grassGroup = grassSystem->grassModelGroup;
            rendererRef.drawScene(*opaquePipeline, grassGroup);

            auto& terrainGroup = grassSystem->terrainModelGroup;
            rendererRef.drawScene(*opaquePipeline, terrainGroup);

            // Copy depth buffer for sampling in effects
            rendererRef.copyDepthForSampling(*depthManager);

            rendererRef.endFrame(imageIndex);
        }

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