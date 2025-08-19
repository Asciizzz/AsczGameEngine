#include "AzCore/Application.hpp"

#include <iostream>
#include <random>

#ifdef NDEBUG // Remember to set this to false
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

    windowManager = MakeUnique<AzCore::WindowManager>(title, width, height);
    fpsManager = MakeUnique<AzCore::FpsManager>();

    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    // 10km view distance for those distant horizons
    camera = MakeUnique<Camera>(glm::vec3(0.0f), 45.0f, 0.01f, 10000.0f);
    camera->setAspectRatio(aspectRatio);

    initVulkan();
}

Application::~Application() {
    cleanup();
}

void Application::run() {
    mainLoop();

    printf("Application exited successfully. See you next time!\n");
}

void Application::initVulkan() {
    auto extensions = windowManager->getRequiredVulkanExtensions();
    vulkanInstance = MakeUnique<Instance>(extensions, enableValidationLayers);
    createSurface();

    vkDevice = MakeUnique<Device>(vulkanInstance->instance, surface);
    VkDevice device = vkDevice->device;
    VkPhysicalDevice physicalDevice = vkDevice->physicalDevice;

    msaaManager = MakeUnique<MSAAManager>(vkDevice.get());
    swapChain = MakeUnique<SwapChain>(vkDevice.get(), surface, windowManager->window);


    // Create shared render pass for forward rendering
    auto renderPassConfig = RenderPassConfig::createForwardRenderingConfig(
        swapChain->imageFormat, msaaManager->msaaSamples
    );
    mainRenderPass = MakeUnique<RenderPass>(device, physicalDevice, renderPassConfig);

    VkRenderPass renderPass = mainRenderPass->renderPass;

    // Initialize render targets and depth testing
    msaaManager->createColorResources(swapChain->extent.width, swapChain->extent.height, swapChain->imageFormat);
    depthManager = MakeUnique<DepthManager>(vkDevice.get());
    depthManager->createDepthResources(swapChain->extent.width, swapChain->extent.height, msaaManager->msaaSamples);
    swapChain->createFramebuffers(renderPass, depthManager->depthImageView, depthManager->depthSamplerView, msaaManager->colorImageView);

    resourceManager = MakeUnique<ResourceManager>(vkDevice.get());
    globalUBOManager = MakeUnique<GlobalUBOManager>(vkDevice.get(), MAX_FRAMES_IN_FLIGHT);

    // Create convenient references to avoid arrow spam
    auto& resManager = *resourceManager;
    auto& texManager = *resManager.textureManager;
    auto& meshManager = *resManager.meshManager;
    auto& matManager = *resManager.materialManager;
    auto& glbUBOManager = *globalUBOManager;    

// PLAYGROUND FROM HERE

    // Set up advanced grass system with terrain generation
    AzGame::GrassConfig grassConfig;
    grassConfig.worldSizeX = 200;
    grassConfig.worldSizeZ = 200;
    grassConfig.baseDensity = 6;
    grassConfig.heightVariance = 26.9f;
    grassConfig.lowVariance = 0.1f;
    grassConfig.numHeightNodes = 1250;
    grassConfig.enableWind = true;
    grassConfig.falloffRadius = 25.0f;
    grassConfig.influenceFactor = 0.02f;
    
    // Initialize grass system
    grassSystem = MakeUnique<AzGame::Grass>(grassConfig);
    grassSystem->initialize(*resourceManager, vkDevice.get());


    // Initialized world
    newWorld = MakeUnique<AzGame::World>(resourceManager.get(), vkDevice.get());

    // Initialize particle system
    particleManager = MakeUnique<AzBeta::ParticleManager>();

    glm::vec3 boundMin = resourceManager->getMesh("TerrainMesh")->nodes[0].min;
    glm::vec3 boundMax = resourceManager->getMesh("TerrainMesh")->nodes[0].max;
    float totalHeight = abs(boundMax.y - boundMin.y);
    boundMin.y -= totalHeight * 2.5f;
    boundMax.y += totalHeight * 12.5f;

    particleManager->initialize(
        resourceManager.get(), vkDevice.get(),
        1000, // Count
        0.5f, // Radius
        0.5f, // Display radius (for objects that seems bigger/smaller than their hitbox)
        boundMin, boundMax
    );



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

        printf("%s   Idx %zu: %s %s-> %sPATH: %s\n", color, index, name.c_str(), WHITE, color, texture->path.c_str());
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

    printf("%s", WHITE);


// PLAYGROUND END HERE 

    meshManager.createBufferDatas();

    matManager.uploadToGPU(MAX_FRAMES_IN_FLIGHT);

    texManager.uploadToGPU(MAX_FRAMES_IN_FLIGHT);

    renderer = MakeUnique<Renderer>(vkDevice.get(), swapChain.get(), depthManager.get(),
                                    globalUBOManager.get(), resourceManager.get());

    using LayoutVec = std::vector<VkDescriptorSetLayout>;
    auto& matDesc = matManager.dynamicDescriptor;
    auto& texDesc = texManager.dynamicDescriptor;
    auto& glbDesc = glbUBOManager.dynamicDescriptor;

    LayoutVec layouts = {glbDesc.setLayout, matDesc.setLayout, texDesc.setLayout};

    opaquePipeline = MakeUnique<Pipeline>(
        device, RasterPipelineConfig::createOpaqueConfig(msaaManager->msaaSamples, renderPass, layouts)
    );
    opaquePipeline->createGraphicPipeline("Shaders/Rasterize/raster.vert.spv", "Shaders/Rasterize/raster.frag.spv");

    transparentPipeline = MakeUnique<Pipeline>(
        device, RasterPipelineConfig::createTransparentConfig(msaaManager->msaaSamples, renderPass, layouts)
    );
    transparentPipeline->createGraphicPipeline("Shaders/Rasterize/raster.vert.spv", "Shaders/Rasterize/raster.frag.spv");

    skyPipeline = MakeUnique<Pipeline>(
        device, RasterPipelineConfig::createSkyConfig(msaaManager->msaaSamples, renderPass, layouts)
    );
    skyPipeline->createGraphicPipeline("Shaders/Sky/sky.vert.spv", "Shaders/Sky/sky.frag.spv");
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

    vkDeviceWaitIdle(vkDevice->device);

    int newWidth, newHeight;
    SDL_GetWindowSize(windowManager->window, &newWidth, &newHeight);

    // Reset like literally everything
    camera->updateAspectRatio(newWidth, newHeight);

    msaaManager->createColorResources(newWidth, newHeight, swapChain->imageFormat);
    depthManager->createDepthResources(newWidth, newHeight, msaaManager->msaaSamples);

    auto& texManager = *resourceManager->textureManager;
    auto& matManager = *resourceManager->materialManager;

    // Recreate render pass with new settings
    auto newRenderPassConfig = RenderPassConfig::createForwardRenderingConfig(
        swapChain->imageFormat, msaaManager->msaaSamples);
    mainRenderPass->recreate(newRenderPassConfig);

    VkRenderPass renderPass = mainRenderPass->renderPass;

    swapChain->recreateFramebuffers(
        windowManager->window, renderPass,
        depthManager->depthImageView,
        depthManager->depthSamplerView,
        msaaManager->colorImageView
    );

    VkSampleCountFlagBits newMsaaSamples = msaaManager->msaaSamples;

    // No need to change layout
    opaquePipeline->recreateGraphicPipeline(renderPass);
    transparentPipeline->recreateGraphicPipeline(renderPass);
    skyPipeline->recreateGraphicPipeline(renderPass);

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

        // Place platform in the world
        static bool hold_g = false;
        if (k_state[SDL_SCANCODE_G] && !hold_g) {
            // Toggle grass instance data update
            Transform trform;
            trform.pos = camRef.pos;

            Model::Data3D instanceData;
            instanceData.modelMatrix = trform.getMat4();

            // grassSystem->addGrassInstance(instanceData);
            newWorld->placePlatformGrid("Ground_x2", camRef.pos);

            hold_g = true;
        } else if (!k_state[SDL_SCANCODE_G]) {
            hold_g = false;
        }

        // Place platform in the world
        static bool hold_p = false;
        static bool particlePhysicsEnabled = false;
        if (k_state[SDL_SCANCODE_P] && !hold_p) {
            // Toggle particle physics
            if (!k_state[SDL_SCANCODE_LSHIFT]) { 
                particlePhysicsEnabled = !particlePhysicsEnabled;
            } else {
                particlePhysicsEnabled = false;

                // Teleport every particle to the current location
                std::vector<Transform>& particles = particleManager->particles;
                std::vector<Model::Data3D>& particlesData = particleManager->particles_data;

                std::vector<size_t> indices(particles.size());
                std::iota(indices.begin(), indices.end(), 0);

                std::for_each(indices.begin(), indices.end(), [&](size_t i) {
                    particles[i].pos = camRef.pos;

                    particlesData[i].modelMatrix = particles[i].getMat4();
                });
            }

            hold_p = true;
        } else if (!k_state[SDL_SCANCODE_P]) {
            hold_p = false;
        }

        if (particlePhysicsEnabled) {
            particleManager->updatePhysic(dTime, resourceManager->getMesh("TerrainMesh"), glm::mat4(1.0f));
        };
        particleManager->updateRender();

// =================================

        // Use the new explicit rendering interface
        globalUBOManager->updateUBO(camRef);

        uint32_t imageIndex = rendererRef.beginFrame(*opaquePipeline, globalUBOManager->ubo);
        if (imageIndex != UINT32_MAX) {
            // First: render sky background with dedicated pipeline
            rendererRef.drawSky(*skyPipeline);

            // Draw grass system
            rendererRef.drawScene(*opaquePipeline, grassSystem->grassFieldModelGroup);
            // Draw the world model group
            rendererRef.drawScene(*opaquePipeline, newWorld->worldModelGroup);
            // Draw the particles
            rendererRef.drawScene(*opaquePipeline, particleManager->particleModelGroup);

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

    vkDeviceWaitIdle(vkDevice->device);
}

void Application::cleanup() {
    // Destroy all Vulkan-resource-owning objects before device destruction
    if (renderer) renderer.reset();
    if (resourceManager) resourceManager.reset();
    if (msaaManager) msaaManager.reset();
    if (depthManager) depthManager.reset();
    if (mainRenderPass) mainRenderPass.reset();
    if (opaquePipeline) opaquePipeline.reset();
    if (transparentPipeline) transparentPipeline.reset();
    if (skyPipeline) skyPipeline.reset();
    if (swapChain) swapChain.reset();

    if (surface != VK_NULL_HANDLE && vulkanInstance) {
        vkDestroySurfaceKHR(vulkanInstance->instance, surface, nullptr);
    }
}