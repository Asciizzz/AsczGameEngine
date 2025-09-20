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
    camera = MakeUnique<Camera>(glm::vec3(0.0f), 45.0f, 0.1f, 1000.0f);
    camera->setAspectRatio(aspectRatio);

    auto extensions = windowManager->getRequiredVulkanExtensions();
    vkInstance = MakeUnique<Instance>(extensions, enableValidationLayers);
    vkInstance->createSurface(windowManager->window);

    deviceVK = MakeUnique<Device>(vkInstance->instance, vkInstance->surface);

    // So we dont have to write these things over and over again
    VkDevice lDevice = deviceVK->lDevice;
    VkPhysicalDevice pDevice = deviceVK->pDevice;

    // Create renderer (which now manages depth manager, swap chain and render passes)
    renderer = MakeUnique<Renderer>(
        deviceVK.get(),
        vkInstance->surface,
        windowManager->window,
        Application::MAX_FRAMES_IN_FLIGHT
    );

    resGroup = MakeUnique<ResourceGroup>(deviceVK.get());
    glbUBOManager = MakeUnique<GlbUBOManager>(
        deviceVK.get(),
        Application::MAX_FRAMES_IN_FLIGHT
    );

// PLAYGROUND FROM HERE

    TinyModel testModel = TinyLoader::loadModel("Assets/Untitled.glb");
    for (auto& mat : testModel.materials) {
        // mat.toonLevel = 4;
    }

    testModel.printAnimationList();

    rigDemo.init(deviceVK.get(), testModel, 0);
    rigDemo.playAnimation(3);

    resGroup->addModel(testModel);

    // TinyLoader::LoadOptions loadOpts;
    // loadOpts.forceStatic = true; // No need for terrain
    // TinyModel testModel2 = TinyLoader::loadModel(".heavy/de_mirage/de_mirage.gltf", loadOpts);
    // for (auto& mat : testModel2.materials) {
    //     mat.shading = false; // No lighting for for highly stylized look
    // }
    // resGroup->addModel(testModel2);

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

    // Initialize dynamic lighting system with example lights
    Az3D::LightVK sunLight{};
    sunLight.position = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // w=0 for directional light
    sunLight.color = glm::vec4(1.0f, 0.9f, 0.8f, 1.5f); // Warm white light with intensity 1.5
    sunLight.direction = glm::vec4(-1.0f, -1.0f, 0.0f, 0.0f); // Direction vector (no range for directional)
    resGroup->addLight(sunLight);

    // Az3D::LightVK pointLight{};
    // pointLight.position = glm::vec4(5.0f, 5.0f, 5.0f, 1.0f); // w=1 for point light
    // pointLight.color = glm::vec4(0.8f, 0.6f, 1.0f, 2.0f); // Purple light with intensity 2.0
    // pointLight.direction = glm::vec4(0.0f, 0.0f, 0.0f, 15.0f); // Range of 15 units
    // pointLight.params = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f); // Attenuation factor 1.0
    // resGroup->addLight(pointLight);

    // Az3D::LightVK spotLight{};
    // spotLight.position = glm::vec4(-10.0f, 8.0f, 0.0f, 2.0f); // w=2 for spot light
    // spotLight.color = glm::vec4(1.0f, 0.5f, 0.2f, 3.0f); // Orange light with intensity 3.0
    // spotLight.direction = glm::vec4(0.7f, -0.5f, 0.0f, 20.0f); // Direction + range of 20 units
    // spotLight.params = glm::vec4(glm::cos(glm::radians(15.0f)), glm::cos(glm::radians(25.0f)), 1.2f, 0.0f); // Inner 15°, outer 25°, attenuation 1.2
    // resGroup->addLight(spotLight);

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
    auto lightLayout = resGroup->getLightDescLayout();

    // Create raster pipeline configurations

    // Create pipeline manager
    pipelineManager = MakeUnique<PipelineManager>();
    
    // Load pipeline configurations from JSON
    if (!pipelineManager->loadPipelinesFromJson("Config/pipelines.json")) {
        std::cout << "Warning: Could not load pipeline JSON, using defaults" << std::endl;
    }

    // Initialize all pipelines with the manager using named layouts
    UnorderedMap<std::string, VkDescriptorSetLayout> namedLayouts = {
        {"global", glbLayout},
        {"material", matLayout}, 
        {"texture", texLayout},
        {"rig", rigLayout},
        {"light", lightLayout}
    };
    
    // Create named vertex inputs
    UnorderedMap<std::string, VertexInputVK> vertexInputVKs;
    
    // None - no vertex input (for fullscreen quads, etc.)
    vertexInputVKs["None"] = VertexInputVK();
    
    // Static - single static mesh
    auto vstaticLayout = TinyVertexStatic::getLayout();
    auto vstaticBind = vstaticLayout.getBindingDescription();
    auto vstaticAttrs = vstaticLayout.getAttributeDescriptions();

    // StaticInstanced - static mesh with instancing
    auto instanceBind = Az3D::StaticInstance::getBindingDescription();
    auto instanceAttrs = Az3D::StaticInstance::getAttributeDescriptions();

    vertexInputVKs["StaticInstanced"] = VertexInputVK()
        .setBindings({vstaticBind, instanceBind})
        .setAttributes({vstaticAttrs, instanceAttrs});
    
    // Rigged - rigged mesh for skeletal animation
    auto vriggedLayout = TinyVertexRig::getLayout();
    auto vriggedBind = vriggedLayout.getBindingDescription();
    auto vriggedAttrs = vriggedLayout.getAttributeDescriptions();

    vertexInputVKs["Rigged"] = VertexInputVK()
        .setBindings({ vriggedBind })
        .setAttributes({ vriggedAttrs });

    vertexInputVKs["Single"] = VertexInputVK()
        .setBindings({ vstaticBind })
        .setAttributes({ vstaticAttrs });
    
    // Use offscreen render pass for pipeline creation
    VkRenderPass offscreenRenderPass = renderer->getOffscreenRenderPass();
    PIPELINE_INIT(pipelineManager.get(), lDevice, offscreenRenderPass, namedLayouts, vertexInputVKs);

    // Load post-process effects from JSON configuration
    renderer->loadPostProcessEffectsFromJson("Config/postprocess.json");
}

void Application::featuresTestingGround() {}

bool Application::checkWindowResize() {
    if (!windowManager->resizedFlag && !renderer->isResizeNeeded()) return false;

    windowManager->resizedFlag = false;
    renderer->setResizeHandled();

    int newWidth, newHeight;
    SDL_GetWindowSize(windowManager->window, &newWidth, &newHeight);

    // Reset like literally everything
    camera->updateAspectRatio(newWidth, newHeight);

    // Handle window resize in renderer (now handles depth resources internally)
    renderer->handleWindowResize(windowManager->window);

    // Recreate all pipelines with offscreen render pass for post-processing
    VkRenderPass offscreenRenderPass = renderer->getOffscreenRenderPass();
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

        uint32_t imageIndex = rendererRef.beginFrame();
        if (imageIndex != UINT32_MAX) {
            // Update global UBO buffer from frame index
            uint32_t currentFrameIndex = rendererRef.getCurrentFrame();
            glbUBOManager->updateUBO(camRef, currentFrameIndex);

            // Update dynamic light buffer if needed
            resGroup->updateLightBuffer();

            // Render sky background with dedicated pipeline
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