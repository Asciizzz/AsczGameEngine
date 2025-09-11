#include "AzCore/Application.hpp"

#include <iostream>
#include <random>

#ifdef NDEBUG // Remember to set this to false
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = true;
#endif

// You're welcome
using namespace AzGame;
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
    camera = MakeUnique<Camera>(glm::vec3(0.0f), 45.0f, 0.1f, 2000.0f);
    camera->setAspectRatio(aspectRatio);

    auto extensions = windowManager->getRequiredVulkanExtensions();
    vkInstance = MakeUnique<Instance>(extensions, enableValidationLayers);
    vkInstance->createSurface(windowManager->window);

    deviceVK = MakeUnique<Device>(vkInstance->instance, vkInstance->surface);

    // So we dont have to write these things over and over again
    VkDevice lDevice = deviceVK->lDevice;
    VkPhysicalDevice pDevice = deviceVK->pDevice;

    swapChain = MakeUnique<SwapChain>(deviceVK.get(), vkInstance->surface, windowManager->window);

    // Create shared render pass for forward rendering
    auto renderPassConfig = RenderPassConfig::createForwardRenderingConfig(
        swapChain->imageFormat
    );
    mainRenderPass = MakeUnique<RenderPass>(lDevice, pDevice, renderPassConfig);

    VkRenderPass renderPass = mainRenderPass->renderPass;

    // Initialize render targets and depth testing
    depthManager = MakeUnique<DepthManager>(deviceVK.get());
    depthManager->createDepthResources(swapChain->extent.width, swapChain->extent.height);
    swapChain->createFramebuffers(renderPass, depthManager->depthImageView);
    renderer = MakeUnique<Renderer>(deviceVK.get(), swapChain.get(), depthManager.get());

    resGroup = MakeUnique<ResourceGroup>(deviceVK.get());
    glbUBOManager = MakeUnique<GlbUBOManager>(deviceVK.get());

// PLAYGROUND FROM HERE

    TinyModel testModel = TinyLoader::loadModel("Assets/Untitled.glb");
    for (auto& mat : testModel.materials) {
        // mat.shading = false; // No lighting for for highly stylized look
        mat.toonLevel = 4;
    }

    testModel.printAnimationList();

    rigDemo.init(deviceVK.get(), testModel, 0);
    rigDemo.playAnimation(3);

    resGroup->addModel(testModel);

    TinyModel testObjModel = TinyLoader::loadModel(".heavy/Town/Tow.obj");
    // testObjModel.printDebug();
    resGroup->addModel(testObjModel);

    // Set up advanced grass system with terrain generation
    GrassConfig grassConfig;
    grassConfig.worldSizeX = 120;
    grassConfig.worldSizeZ = 120;
    grassConfig.baseDensity = 3;
    grassConfig.heightVariance = 26.9f;
    grassConfig.lowVariance = 0.1f;
    grassConfig.numHeightNodes = 750;
    grassConfig.enableWind = true;
    grassConfig.falloffRadius = 25.0f;
    grassConfig.influenceFactor = 0.02f;

    grassSystem = MakeUnique<Grass>(grassConfig);
    grassSystem->initialize(*resGroup, deviceVK.get());

    // Initialize particle system
    particleManager = MakeUnique<AzBeta::ParticleManager>();

    // glm::vec3 boundMin = resGroup->getStaticMesh("TerrainMesh")->nodes[0].min;
    // glm::vec3 boundMax = resGroup->getStaticMesh("TerrainMesh")->nodes[0].max;
    // float totalHeight = abs(boundMax.y - boundMin.y);
    // boundMin.y -= totalHeight * 2.5f;
    // boundMax.y += totalHeight * 12.5f;

    // particleManager->initialize(
    //     resGroup.get(), deviceVK.get(),
    //     5000, // Count
    //     0.5f, // Radius
    //     0.5f, // Display radius
    //     boundMin, boundMax
    // );

// PLAYGROUND END HERE 

    resGroup->uploadAllToGPU();


    auto glbLayout = glbUBOManager->getDescLayout();
    auto matLayout = resGroup->getMatDescLayout();
    auto texLayout = resGroup->getTexDescLayout();
    auto rigLayout = resGroup->getRigDescLayout();

    // Create raster pipeline configurations

    // Create pipeline manager
    pipelineManager = MakeUnique<PipelineManager>();
    
    // Load pipeline configurations from JSON
    if (!pipelineManager->loadPipelinesFromJson("Shaders/pipelines.json")) {
        std::cout << "Warning: Could not load pipeline JSON, using defaults" << std::endl;
    }

    // Initialize all pipelines with the manager using named layouts
    UnorderedMap<std::string, VkDescriptorSetLayout> namedLayouts = {
        {"global", glbLayout},
        {"material", matLayout}, 
        {"texture", texLayout},
        {"rig", rigLayout}
    };
    
    // Create named vertex inputs
    UnorderedMap<std::string, VertexInputVK> vertexInputVKs;
    
    // None - no vertex input (for fullscreen quads, etc.)
    VertexInputVK noneInput;
    vertexInputVKs["None"] = noneInput;
    
    // Static - single static mesh
    auto vstaticLayout = TinyVertexStatic::getLayout();
    auto vstaticBind = vstaticLayout.getBindingDescription();
    auto vstaticAttrs = vstaticLayout.getAttributeDescriptions();

    // StaticInstanced - static mesh with instancing
    auto instanceBind = Az3D::StaticInstance::getBindingDescription();
    auto instanceAttrs = Az3D::StaticInstance::getAttributeDescriptions();
    VertexInputVK vstaticInstancedInput = VertexInputVK()
    .setBindings({vstaticBind, instanceBind})
    .setAttributes({vstaticAttrs, instanceAttrs});
    vertexInputVKs["StaticInstanced"] = vstaticInstancedInput;
    
    // Rigged - rigged mesh for skeletal animation
    auto vriggedLayout = TinyVertexRig::getLayout();
    auto vriggedBind = vriggedLayout.getBindingDescription();
    auto vriggedAttrs = vriggedLayout.getAttributeDescriptions();
    VertexInputVK vriggedInput = VertexInputVK()
    .setBindings({ vriggedBind })
    .setAttributes({ vriggedAttrs });
    vertexInputVKs["Rigged"] = vriggedInput;

    // Single - single static mesh (alias for Static)
    VertexInputVK vsingleInput = VertexInputVK()
    .setBindings({ vstaticBind })
    .setAttributes({ vstaticAttrs });
    vertexInputVKs["Single"] = vsingleInput;
    
    // Use offscreen render pass for pipeline creation
    VkRenderPass offscreenRenderPass = renderer->postProcess->getOffscreenRenderPass();
    PIPELINE_INIT(pipelineManager.get(), lDevice, offscreenRenderPass, namedLayouts, vertexInputVKs);

    renderer->addPostProcessEffect("fxaa", "Shaders/PostProcess/fxaa.comp.spv");
    renderer->addPostProcessEffect("tonemap", "Shaders/PostProcess/tonemap.comp.spv");
}

void Application::featuresTestingGround() {}

bool Application::checkWindowResize() {
    if (!windowManager->resizedFlag && !renderer->framebufferResized) return false;

    windowManager->resizedFlag = false;
    renderer->framebufferResized = false;

    vkDeviceWaitIdle(deviceVK->lDevice);

    int newWidth, newHeight;
    SDL_GetWindowSize(windowManager->window, &newWidth, &newHeight);

    // Reset like literally everything
    camera->updateAspectRatio(newWidth, newHeight);

    depthManager->createDepthResources(newWidth, newHeight);

    // Recreate render pass with new settings
    auto newRenderPassConfig = RenderPassConfig::createForwardRenderingConfig(
        swapChain->imageFormat
    );
    mainRenderPass->recreate(newRenderPassConfig);

    VkRenderPass renderPass = mainRenderPass->get();
    swapChain->recreateFramebuffers(
        windowManager->window, renderPass,
        depthManager->depthImageView
    );

    // Recreate post-processing resources for new swapchain size
    renderer->postProcess->recreate();

    // Recreate all pipelines with offscreen render pass for post-processing
    VkRenderPass offscreenRenderPass = renderer->postProcess->getOffscreenRenderPass();
    pipelineManager->recreateAllPipelines(offscreenRenderPass);


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
            float yawDelta = -mouseX * sensitivity;  // Inverted for correct quaternion rotation
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

        // Camera roll controls (Q/E keys)
        float rollSpeed = 45.0f * dTime; // 45 degrees per second
        if (k_state[SDL_SCANCODE_Q]) camRef.rotateRoll(-rollSpeed);
        if (k_state[SDL_SCANCODE_E]) camRef.rotateRoll(rollSpeed);

        // Reset roll with R key
        if (k_state[SDL_SCANCODE_R]) {
            camRef.rotateRoll(camRef.getRoll() * dTime * 10.0f);
        }

        camRef.pos = camPos;

        // Update grass wind animation
        static bool yPressed = false;
        static bool enable_wind = false;
        static bool use_gpu = true;
        if (k_state[SDL_SCANCODE_Y] && !yPressed) {

            if (k_state[SDL_SCANCODE_LSHIFT]) {
                use_gpu = !use_gpu;
            } else {
                enable_wind = !enable_wind;
            }

            yPressed = true;
        } else if (!k_state[SDL_SCANCODE_Y]) {
            yPressed = false;
        }

        if (grassSystem && enable_wind) {
            grassSystem->updateWindAnimation(dTime, use_gpu);
        }

        // // Particle physics toggle and teleport
        // static bool pPressed = false;
        // static bool particlePhysicsEnabled = false;
        // if (k_state[SDL_SCANCODE_P] && !pPressed) {
        //     // Toggle particle physics
        //     if (!k_state[SDL_SCANCODE_LSHIFT]) { 
        //         particlePhysicsEnabled = !particlePhysicsEnabled;
        //     } else {
        //         particlePhysicsEnabled = false;

        //         // Teleport every particle to the current location
        //         std::vector<Transform>& particles = particleManager->particles;
        //         std::vector<StaticInstance>& particlesData = particleManager->particles_data;

        //         std::vector<size_t> indices(particles.size());
        //         std::iota(indices.begin(), indices.end(), 0);

        //         std::for_each(indices.begin(), indices.end(), [&](size_t i) {
        //             particles[i].pos = camRef.pos;

        //             particlesData[i].setTransform(particles[i].pos, particles[i].rot);
        //         });
        //     }

        //     pPressed = true;
        // } else if (!k_state[SDL_SCANCODE_P]) {
        //     pPressed = false;
        // }

        // if (particlePhysicsEnabled) {
        //     // particleManager->updatePhysic(dTime, resGroup->getStaticMesh("TerrainMesh"), glm::mat4(1.0f));
        // };
        // particleManager->updateRender();

        // Crouch walking
        if (k_state[SDL_SCANCODE_1]) rigDemo.playAnimation(0);
        if (k_state[SDL_SCANCODE_2]) rigDemo.playAnimation(1);
        if (k_state[SDL_SCANCODE_3]) rigDemo.playAnimation(2);
        if (k_state[SDL_SCANCODE_4]) rigDemo.playAnimation(3);
        if (k_state[SDL_SCANCODE_5]) rigDemo.playAnimation(4);
        if (k_state[SDL_SCANCODE_6]) rigDemo.playAnimation(5);


// =================================

        // grassSystem->grassInstanceGroup.updateDataBuffer(); // Per frame update since grass moves
        // Use the new explicit rendering interface
        
        uint32_t imageIndex = rendererRef.beginFrame(mainRenderPass->get());
        if (imageIndex != UINT32_MAX) {
            // Update global UBO buffer from frame index
            uint32_t currentFrameIndex = rendererRef.getCurrentFrame();
            glbUBOManager->updateUBO(camRef, currentFrameIndex);

            // First: render sky background with dedicated pipeline
            rendererRef.drawSky(glbUBOManager.get(), PIPELINE_INSTANCE(pipelineManager.get(), "Sky"));

            // Draw grass system
            grassSystem->grassInstanceGroup.updateDataBuffer(); // Per frame update since grass moves
            rendererRef.drawStaticInstanceGroup(resGroup.get(), glbUBOManager.get(), PIPELINE_INSTANCE(pipelineManager.get(), "Foliage"), &grassSystem->grassInstanceGroup);
            rendererRef.drawStaticInstanceGroup(resGroup.get(), glbUBOManager.get(), PIPELINE_INSTANCE(pipelineManager.get(), "StaticMesh"), &grassSystem->terrainInstanceGroup);

            rigDemo.update(dTime);
            rendererRef.drawDemoRig(resGroup.get(), glbUBOManager.get(), PIPELINE_INSTANCE(pipelineManager.get(), "RiggedMesh"), rigDemo);

            rendererRef.drawSingleInstance(resGroup.get(), glbUBOManager.get(), PIPELINE_INSTANCE(pipelineManager.get(), "Single"), 1);

            // Draw the particles
            // particleManager->instanceGroup.updateDataBuffer();
            // rendererRef.drawStaticInstanceGroup(resGroup.get(), glbUBOManager.get(), PIPELINE_INSTANCE(pipelineManager.get(), "StaticMesh"), &particleManager->instanceGroup);

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
                                                std::to_string(camRef.pos.z) + " | " +
                                    " | Forward: " + std::to_string(static_cast<int>(camRef.forward.x * 100) / 100.0f) + ", " +
                                                    std::to_string(static_cast<int>(camRef.forward.y * 100) / 100.0f) + ", " +
                                                    std::to_string(static_cast<int>(camRef.forward.z * 100) / 100.0f) + ", " +
                                    " | Right: " + std::to_string(static_cast<int>(camRef.right.x * 100) / 100.0f) + ", " +
                                                    std::to_string(static_cast<int>(camRef.right.y * 100) / 100.0f) + ", " +
                                                    std::to_string(static_cast<int>(camRef.right.z * 100) / 100.0f);
            SDL_SetWindowTitle(winManager.window, fpsText.c_str());
            lastFpsOutput = now;
        }
    }

    vkDeviceWaitIdle(deviceVK->lDevice);
}

void Application::cleanup() {}