#include "AzCore/Application.hpp"

#include "AzVulk/ComputeTask.hpp"

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

    initComponents();
    featuresTestingGround();
}

Application::~Application() {
    cleanup();
}

void Application::run() {
    mainLoop();

    printf("Application exited successfully. See you next time!\n");
}

void Application::initComponents() {

    windowManager = MakeUnique<AzCore::WindowManager>(appTitle, appWidth, appHeight);
    fpsManager = MakeUnique<AzCore::FpsManager>();

    float aspectRatio = static_cast<float>(appWidth) / static_cast<float>(appHeight);
    // 10km view distance for those distant horizons
    camera = MakeUnique<Camera>(glm::vec3(0.0f), 45.0f, 0.01f, 10000.0f);
    camera->setAspectRatio(aspectRatio);

    auto extensions = windowManager->getRequiredVulkanExtensions();
    vkInstance = MakeUnique<Instance>(extensions, enableValidationLayers);
    vkInstance->createSurface(windowManager->window);

    vkDevice = MakeUnique<Device>(vkInstance->instance, vkInstance->surface);

    // So we dont have to write these things over and over again
    VkDevice lDevice = vkDevice->lDevice;
    VkPhysicalDevice pDevice = vkDevice->pDevice;

    msaaManager = MakeUnique<MSAAManager>(vkDevice.get());
    swapChain = MakeUnique<SwapChain>(vkDevice.get(), vkInstance->surface, windowManager->window);


    // Create shared render pass for forward rendering
    auto renderPassConfig = RenderPassConfig::createForwardRenderingConfig(
        swapChain->imageFormat, msaaManager->msaaSamples
    );
    mainRenderPass = MakeUnique<RenderPass>(lDevice, pDevice, renderPassConfig);

    VkRenderPass renderPass = mainRenderPass->renderPass;

    // Initialize render targets and depth testing
    msaaManager->createColorResources(swapChain->extent.width, swapChain->extent.height, swapChain->imageFormat);
    depthManager = MakeUnique<DepthManager>(vkDevice.get());
    depthManager->createDepthResources(swapChain->extent.width, swapChain->extent.height, msaaManager->msaaSamples);
    swapChain->createFramebuffers(renderPass, depthManager->depthImageView, msaaManager->colorImageView);
    renderer = MakeUnique<Renderer>(vkDevice.get(), swapChain.get());

    resGroup = MakeUnique<ResourceGroup>(vkDevice.get());
    glbUBOManager = MakeUnique<GlbUBOManager>(vkDevice.get());

// PLAYGROUND FROM HERE

    // Set up advanced grass system with terrain generation
    AzGame::GrassConfig grassConfig;
    grassConfig.worldSizeX = 170;
    grassConfig.worldSizeZ = 170;
    grassConfig.baseDensity = 6;
    grassConfig.heightVariance = 26.9f;
    grassConfig.lowVariance = 0.1f;
    grassConfig.numHeightNodes = 750;
    grassConfig.enableWind = true;
    grassConfig.falloffRadius = 25.0f;
    grassConfig.influenceFactor = 0.02f;
    
    // Initialize grass system
    grassSystem = MakeUnique<AzGame::Grass>(grassConfig);
    grassSystem->initialize(*resGroup, vkDevice.get());


    // Initialized world
    newWorld = MakeUnique<AzGame::World>(resGroup.get(), vkDevice.get());

    // Initialize particle system
    particleManager = MakeUnique<AzBeta::ParticleManager>();

    glm::vec3 boundMin = resGroup->getStaticMesh("TerrainMesh")->nodes[0].min;
    glm::vec3 boundMax = resGroup->getStaticMesh("TerrainMesh")->nodes[0].max;
    float totalHeight = abs(boundMax.y - boundMin.y);
    boundMin.y -= totalHeight * 2.5f;
    boundMax.y += totalHeight * 12.5f;

    particleManager->initialize(
        resGroup.get(), vkDevice.get(),
        5000, // Count
        0.5f, // Radius
        0.5f, // Display radius
        boundMin, boundMax
    );

    resGroup->addRiggedModel("Demo", "Assets/Characters/NewSelen.gltf");

    rigDemo = MakeUnique<Az3D::RigDemo>();
    rigDemo->init(vkDevice.get(), resGroup->rigSkeletons[0]);
    rigDemo->meshIndex = 0;

// PLAYGROUND END HERE 

    resGroup->uploadAllToGPU();

    auto glbLayout = glbUBOManager->getDescLayout();
    auto matLayout = resGroup->getMatDescLayout();
    auto texLayout = resGroup->getTexDescLayout();
    auto rigLayout = resGroup->getRigDescLayout();

    // Create raster pipeline configurations

    RasterCfg staticMeshConfig;
    staticMeshConfig.renderPass = renderPass;
    staticMeshConfig.setMSAA(msaaManager->msaaSamples);
    staticMeshConfig.vertexInputType = RasterCfg::InputType::Static;
    staticMeshConfig.setLayouts = {glbLayout, matLayout, texLayout};
    staticMeshConfig.vertPath = "Shaders/Rasterize/StaticMesh.vert.spv";
    staticMeshConfig.fragPath = "Shaders/Rasterize/StaticMesh.frag.spv";

    staticMeshPipeline = MakeUnique<PipelineRaster>(lDevice, staticMeshConfig);
    staticMeshPipeline->create();

    RasterCfg rigMeshConfig;
    rigMeshConfig.renderPass = renderPass;
    rigMeshConfig.setMSAA(msaaManager->msaaSamples);
    rigMeshConfig.vertexInputType = RasterCfg::InputType::Rigged;
    rigMeshConfig.setLayouts = {glbLayout, matLayout, texLayout, rigLayout};
    rigMeshConfig.vertPath = "Shaders/Rasterize/RigMesh.vert.spv";
    rigMeshConfig.fragPath = "Shaders/Rasterize/RigMesh.frag.spv";
    rigMeshConfig.cullMode = VK_CULL_MODE_NONE;
    // rigMeshConfig.frontFace = VK_FRONT_FACE_CLOCKWISE;

    rigMeshPipeline = MakeUnique<PipelineRaster>(lDevice, rigMeshConfig);
    rigMeshPipeline->create();

    RasterCfg skyConfig;
    skyConfig.renderPass = renderPass;
    skyConfig.setMSAA(msaaManager->msaaSamples);
    skyConfig.vertexInputType = RasterCfg::InputType::None;
    skyConfig.setLayouts = {glbLayout};
    skyConfig.vertPath = "Shaders/Sky/sky.vert.spv";
    skyConfig.fragPath = "Shaders/Sky/sky.frag.spv";

    skyConfig.cullMode = VK_CULL_MODE_NONE;           // No culling for fullscreen quad
    skyConfig.depthTestEnable = VK_FALSE;             // Sky is always furthest
    skyConfig.depthWriteEnable = VK_FALSE;            // Don't write depth
    skyConfig.depthCompareOp = VK_COMPARE_OP_ALWAYS;  // Always pass depth test
    skyConfig.blendEnable = VK_FALSE;                 // No blending needed

    skyPipeline = MakeUnique<PipelineRaster>(lDevice, skyConfig);
    skyPipeline->create();


    RasterCfg foliageConfig;
    foliageConfig.renderPass = renderPass;
    foliageConfig.setMSAA(msaaManager->msaaSamples);
    foliageConfig.vertexInputType = RasterCfg::InputType::Static;
    foliageConfig.setLayouts = {glbLayout, matLayout, texLayout};
    foliageConfig.vertPath = "Shaders/Rasterize/StaticMesh.vert.spv";
    foliageConfig.fragPath = "Shaders/Rasterize/StaticMesh.frag.spv";

    // No backface culling
    foliageConfig.cullMode = VK_CULL_MODE_NONE;

    foliagePipeline = MakeUnique<PipelineRaster>(lDevice, foliageConfig);
    foliagePipeline->create();
}

void Application::featuresTestingGround() {

    size_t dataCount = 120;

    std::vector<glm::mat4> dataA(dataCount); // Result mat4x4
    std::vector<glm::mat4> dataB(dataCount); // Data mat4x4 1
    std::vector<glm::mat4> dataC(dataCount); // Data mat4x4 2
    std::vector<float> dataD(dataCount);     // Data float
    float scalarE = 2.0f;                    // Data scalar

    for (size_t i = 0; i < dataCount; ++i) {
        dataB[i] = glm::mat4(static_cast<float>(rand()) / RAND_MAX);
        dataC[i] = glm::mat4(static_cast<float>(rand()) / RAND_MAX);
        dataD[i] = static_cast<float>(rand()) / RAND_MAX;
    }

    // Measure CPU time
    auto cpuStart = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < dataCount; ++i) {
        dataA[i] = (dataB[i] * dataC[i]) * dataD[i] * scalarE;
    }

    auto cpuEnd = std::chrono::high_resolution_clock::now();
    double cpuDuration = std::chrono::duration<double, std::milli>(cpuEnd - cpuStart).count();

    std::cout << "CPU elapsed time: " << cpuDuration << " ms\n";


    auto gpuStart = std::chrono::high_resolution_clock::now();

    // Create buffer
    BufferData bufA(vkDevice.get()), bufB(vkDevice.get()), bufC(vkDevice.get()), bufD(vkDevice.get()), bufE(vkDevice.get());

    ComputeTask::makeStorageBuffer(bufA, dataA.data(), sizeof(glm::mat4) * dataCount);
    ComputeTask::makeStorageBuffer(bufB, dataB.data(), sizeof(glm::mat4) * dataCount);
    ComputeTask::makeStorageBuffer(bufC, dataC.data(), sizeof(glm::mat4) * dataCount);
    ComputeTask::makeStorageBuffer(bufD, dataD.data(), sizeof(float)     * dataCount);
    ComputeTask::makeUniformBuffer(bufE, &scalarE, sizeof(float));

    ComputeTask compTask(vkDevice.get(), "Shaders/Compute/test.comp.spv");
    compTask.addStorageBuffer(bufA, 0);
    compTask.addStorageBuffer(bufB, 1);
    compTask.addStorageBuffer(bufC, 2);
    compTask.addStorageBuffer(bufD, 3);
    compTask.addUniformBuffer(bufE, 4);
    compTask.create();

    compTask.dispatchAsync(static_cast<uint32_t>(dataCount), 512);

    // Get the result
    // glm::mat4* finalA = reinterpret_cast<glm::mat4*>(bufA.mapped);
    // // Copy the result to dataD
    // std::memcpy(dataA.data(), finalA, dataCount * sizeof(glm::mat4));
    ComputeTask::fetchResults(bufA, dataA.data(), sizeof(glm::mat4) * dataCount);

    auto gpuEnd = std::chrono::high_resolution_clock::now();
    double gpuDuration = std::chrono::duration<double, std::milli>(gpuEnd - gpuStart).count();

    std::cout << "GPU elapsed time: " << gpuDuration << " ms\n";

    // Automatic cleanup
}

bool Application::checkWindowResize() {
    if (!windowManager->resizedFlag && !renderer->framebufferResized) return false;

    windowManager->resizedFlag = false;
    renderer->framebufferResized = false;

    vkDeviceWaitIdle(vkDevice->lDevice);

    int newWidth, newHeight;
    SDL_GetWindowSize(windowManager->window, &newWidth, &newHeight);

    // Reset like literally everything
    camera->updateAspectRatio(newWidth, newHeight);

    msaaManager->createColorResources(newWidth, newHeight, swapChain->imageFormat);
    depthManager->createDepthResources(newWidth, newHeight, msaaManager->msaaSamples);


    // Recreate render pass with new settings
    auto newRenderPassConfig = RenderPassConfig::createForwardRenderingConfig(
        swapChain->imageFormat, msaaManager->msaaSamples);
    mainRenderPass->recreate(newRenderPassConfig);

    VkRenderPass renderPass = mainRenderPass->get();

    swapChain->recreateFramebuffers(
        windowManager->window, renderPass,
        depthManager->depthImageView,
        msaaManager->colorImageView
    );

    // No need to change layout
    staticMeshPipeline->setRenderPass(renderPass);
    staticMeshPipeline->recreate();

    rigMeshPipeline->setRenderPass(renderPass);
    rigMeshPipeline->recreate();

    skyPipeline->setRenderPass(renderPass);
    skyPipeline->recreate();

    foliagePipeline->setRenderPass(renderPass);
    foliagePipeline->recreate();

    return true;
}


void Application::mainLoop() {
    SDL_SetRelativeMouseMode(SDL_TRUE); // Start with mouse locked

    // Create references to avoid arrow spam
    auto& winManager = *windowManager;
    auto& fpsRef = *fpsManager;

    auto& camRef = *camera;

    auto& rendererRef = *renderer;

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
        static bool use_gpu = true;
        if (k_state[SDL_SCANCODE_Y] && !hold_y) {

            if (k_state[SDL_SCANCODE_LSHIFT]) {
                use_gpu = !use_gpu;
            } else {
                enable_wind = !enable_wind;
            }

            hold_y = true;
        } else if (!k_state[SDL_SCANCODE_Y]) {
            hold_y = false;
        }

        if (grassSystem && enable_wind) {
            grassSystem->updateWindAnimation(dTime, use_gpu);
        }

        // Place platform in the world
        static bool hold_g = false;
        if (k_state[SDL_SCANCODE_G] && !hold_g) {
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
                std::vector<StaticInstance>& particlesData = particleManager->particles_data;

                std::vector<size_t> indices(particles.size());
                std::iota(indices.begin(), indices.end(), 0);

                std::for_each(indices.begin(), indices.end(), [&](size_t i) {
                    particles[i].pos = camRef.pos;

                    particlesData[i].setTransform(particles[i].pos, particles[i].rot);
                });
            }

            hold_p = true;
        } else if (!k_state[SDL_SCANCODE_P]) {
            hold_p = false;
        }

        if (particlePhysicsEnabled) {
            particleManager->updatePhysic(dTime, resGroup->getStaticMesh("TerrainMesh"), glm::mat4(1.0f));
        };
        particleManager->updateRender();

// =================================

        // Use the new explicit rendering interface
        glbUBOManager->updateUBO(camRef);

        uint32_t imageIndex = rendererRef.beginFrame(mainRenderPass->get(), msaaManager->hasMSAA);
        if (imageIndex != UINT32_MAX) {
            // First: render sky background with dedicated pipeline
            rendererRef.drawSky(glbUBOManager.get(), skyPipeline.get());

            // Draw grass system

            // No need for per frame terrain update since it never moves
            rendererRef.drawStaticInstanceGroup(resGroup.get(), glbUBOManager.get(), staticMeshPipeline.get(), &grassSystem->terrainInstanceGroup);

            // ================= DEMO RIG WORKED HOLY SHIT ARE YOU PROUD OF ME =======================

            rigDemo->funFunction(dTime);
            rigDemo->updateBuffer();

            rendererRef.drawDemoRig(resGroup.get(), glbUBOManager.get(), rigMeshPipeline.get(), rigDemo.get());

            // grassSystem->grassInstanceGroup.updateBufferData(); // Per frame update since grass moves
            // rendererRef.drawStaticInstanceGroup(resGroup.get(), foliagePipeline.get(), &grassSystem->grassInstanceGroup);

            // Draw the particles
            // particleManager->instanceGroup.updateBufferData();
            // rendererRef.drawStaticInstanceGroup(resGroup.get(), glbUBOManager.get(), staticMeshPipeline.get(), &particleManager->instanceGroup);

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

    vkDeviceWaitIdle(vkDevice->lDevice);
}

void Application::cleanup() {

}