#include "AzCore/Application.hpp"

#include <iostream>
#include <random>

#ifdef NDEBUG
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = true;
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
    
    // Some very repetitive vulkan stuff
    VkDevice device = vulkanDevice->device;
    VkRenderPass renderPass = mainRenderPass->renderPass;

    // Create descriptor manager and both set layouts
    descriptorManager = std::make_unique<DescriptorManager>(device);
    descriptorManager->createDescriptorSetLayouts(2);

    auto& matDesc = descriptorManager->materialDynamicDescriptor;
    auto& glbDesc = descriptorManager->globalDynamicDescriptor;
    auto& texDesc = descriptorManager->textureDynamicDescriptor;

    using LayoutVec = std::vector<VkDescriptorSetLayout>;

    // Use both layouts for all pipelines
    opaquePipeline = std::make_unique<RasterPipeline>(
        device, renderPass,
        LayoutVec{glbDesc.setLayout, matDesc.setLayout, texDesc.setLayout},
        "Shaders/Rasterize/raster.vert.spv",
        "Shaders/Rasterize/raster.frag.spv",
        RasterPipelineConfig::createOpaqueConfig(msaaManager->msaaSamples)
    );

    transparentPipeline = std::make_unique<RasterPipeline>(
        device, renderPass,
        LayoutVec{glbDesc.setLayout, matDesc.setLayout, texDesc.setLayout},
        "Shaders/Rasterize/raster.vert.spv",
        "Shaders/Rasterize/raster.frag.spv",
        RasterPipelineConfig::createTransparentConfig(msaaManager->msaaSamples)
    );

    skyPipeline = std::make_unique<RasterPipeline>(
        device, renderPass,
        LayoutVec{glbDesc.setLayout},
        "Shaders/Sky/sky.vert.spv",
        "Shaders/Sky/sky.frag.spv",
        RasterPipelineConfig::createSkyConfig(msaaManager->msaaSamples)
    );

    shaderManager = std::make_unique<ShaderManager>(device);

    // Create command pool for graphics operations
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = vulkanDevice->queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }

    // Initialize render targets and depth testing
    msaaManager->createColorResources(swapChain->extent.width, swapChain->extent.height, swapChain->imageFormat);
    depthManager = std::make_unique<DepthManager>(*vulkanDevice);
    depthManager->createDepthResources(swapChain->extent.width, swapChain->extent.height, msaaManager->msaaSamples);
    swapChain->createFramebuffers(renderPass, depthManager->depthImageView, msaaManager->colorImageView);

    buffer = std::make_unique<Buffer>(*vulkanDevice);
    buffer->createUniformBuffers(2);
    // Create dummy vertex buffer for sky pass (fullscreen, no attributes)
    buffer->createDummyVertexBuffer();

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

        {"Water_x2", "water_2.obj"},
        {"Water_x4", "water_4.obj"},
        
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

    ModelGroup worldGroup("WorldGroup");

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


    worldGroup.buildMeshMapping();

    mdlManager.addGroup("World", std::move(worldGroup));


    // Set up advanced grass system with terrain generation
    AzGame::GrassConfig grassConfig;
    grassConfig.worldSizeX = 64;
    grassConfig.worldSizeZ = 64;
    grassConfig.baseDensity = 4;
    grassConfig.heightVariance = 3.9f;
    grassConfig.lowVariance = 0.1f;
    grassConfig.numHeightNodes = 100;
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

    // Create descriptor pools and sets (split global/material)
    size_t matCount = matManager.materials.size();
    size_t texCount = texManager.textures.size();

    descriptorManager->createDescriptorPools(matCount, texCount);
    glbDesc.createGlobalDescriptorSets(
        bufferRef.uniformBuffers, sizeof(GlobalUBO),
        depthManager->depthSamplerView, depthManager->depthSampler
    );
    // for (size_t i = 0; i < matManager.materials.size(); ++i) {
    //     VkBuffer materialUniformBuffer = bufferRef.getMaterialUniformBuffer(i);
    //     size_t textureIndex = matManager.materials[i]->diffTxtr;
    //     matDesc.createMaterialDescriptorSets_LEGACY(
    //         &texManager.textures[textureIndex], materialUniformBuffer, i
    //     );
    // }
    matDesc.createMaterialDescriptorSets(
        matManager.materials, bufferRef.materialUniformBuffers
    );
    texDesc.createTextureDescriptorSets(texManager.textures);


    // Load meshes into GPU buffer
    for (size_t i = 0; i < meshManager.meshes.size(); ++i) {
        bufferRef.loadMeshToBuffer(*meshManager.meshes[i]);
    }

    // Final Renderer setup with ResourceManager
    renderer = std::make_unique<Renderer>(*vulkanDevice, *swapChain, *buffer,
                                        *descriptorManager, *resourceManager, depthManager.get());
}

void Application::createSurface() {
    if (!SDL_Vulkan_CreateSurface(windowManager->window, vulkanInstance->instance, &surface)) {
        throw std::runtime_error("failed to create window surface!");
    }
}

bool Application::checkWindowResize() {
    if (!windowManager->resizedFlag && !renderer->framebufferResized) return false;

    windowManager->resizedFlag = false;
    renderer->framebufferResized = false;

    vkDeviceWaitIdle(vulkanDevice->device);

    int newWidth, newHeight;
    SDL_GetWindowSize(windowManager->window, &newWidth, &newHeight);

    // Reset like literally everything
    camera->updateAspectRatio(newWidth, newHeight);

    msaaManager->createColorResources(newWidth, newHeight, swapChain->imageFormat);
    depthManager->createDepthResources(newWidth, newHeight, msaaManager->msaaSamples);

    auto& bufferRef = *buffer;
    auto& texManager = *resourceManager->textureManager;
    auto& matManager = *resourceManager->materialManager;

    // TODO: Currently the global depth sampler is still using the outdated depth sampler

    auto& glbDesc = descriptorManager->globalDynamicDescriptor;
    // glbDesc.createSetLayout({
    //     DynamicDescriptor::fastBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
    //     DynamicDescriptor::fastBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    // });

    // glbDesc.createPool(1, {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER});

    glbDesc.createGlobalDescriptorSets(
        bufferRef.uniformBuffers, sizeof(GlobalUBO),
        depthManager->depthSamplerView, depthManager->depthSampler
    );

    // Recreate render pass with new settings
    auto newRenderPassConfig = RenderPassConfig::createForwardRenderingConfig(
        swapChain->imageFormat, msaaManager->msaaSamples);
    mainRenderPass->recreate(newRenderPassConfig);

    VkRenderPass renderPass = mainRenderPass->renderPass;

    swapChain->recreate(windowManager->window, renderPass, depthManager->depthImageView, msaaManager->colorImageView);

    VkSampleCountFlagBits newMsaaSamples = msaaManager->msaaSamples;

    // Since descriptor receive no changes, we no need to change the descriptor in the pipeline
    opaquePipeline->recreate(renderPass, RasterPipelineConfig::createOpaqueConfig(newMsaaSamples));
    transparentPipeline->recreate(renderPass, RasterPipelineConfig::createTransparentConfig(newMsaaSamples));
    skyPipeline->recreate(renderPass, RasterPipelineConfig::createSkyConfig(newMsaaSamples));

    return true;
}


void Application::mainLoop() {
    SDL_SetRelativeMouseMode(SDL_TRUE); // Start with mouse locked

    // Create references to avoid arrow spam
    auto& winManager = *windowManager;
    auto& fpsRef = *fpsManager;
    auto& camRef = *camera;

    auto& rendererRef = *renderer;

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
        checkWindowResize();

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

            rendererRef.endFrame(imageIndex);
        };

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

    vkDeviceWaitIdle(vulkanDevice->device);
}

void Application::cleanup() {
    // Destroy all Vulkan-resource-owning objects before device destruction
    if (renderer) renderer.reset();
    if (resourceManager) resourceManager.reset();
    if (buffer) buffer.reset();
    if (descriptorManager) descriptorManager.reset();
    if (msaaManager) msaaManager.reset();
    if (depthManager) depthManager.reset();
    if (mainRenderPass) mainRenderPass.reset();
    if (opaquePipeline) opaquePipeline.reset();
    if (transparentPipeline) transparentPipeline.reset();
    if (skyPipeline) skyPipeline.reset();
    if (shaderManager) shaderManager.reset();
    if (swapChain) swapChain.reset();

    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(vulkanDevice->device, commandPool, nullptr);
    }

    if (surface != VK_NULL_HANDLE && vulkanInstance) {
        vkDestroySurfaceKHR(vulkanInstance->instance, surface, nullptr);
    }
}