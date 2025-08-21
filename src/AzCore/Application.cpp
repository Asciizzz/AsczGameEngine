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
    VkDevice device = vkDevice->device;
    VkPhysicalDevice physicalDevice = vkDevice->physicalDevice;

    msaaManager = MakeUnique<MSAAManager>(vkDevice.get());
    swapChain = MakeUnique<SwapChain>(vkDevice.get(), vkInstance->surface, windowManager->window);


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

    RasterPipelineConfig opaqueConfig;
    opaqueConfig.renderPass = renderPass;
    opaqueConfig.msaaSamples = msaaManager->msaaSamples;
    opaqueConfig.setLayouts = layouts;
    opaqueConfig.vertPath = "Shaders/Rasterize/raster.vert.spv";
    opaqueConfig.fragPath = "Shaders/Rasterize/raster.frag.spv";

    opaquePipeline = MakeUnique<GraphicsPipeline>(device, opaqueConfig);
    opaquePipeline->create();

    RasterPipelineConfig skyConfig;
    skyConfig.renderPass = renderPass;
    skyConfig.msaaSamples = msaaManager->msaaSamples;
    skyConfig.setLayouts = layouts;
    skyConfig.vertPath = "Shaders/Sky/sky.vert.spv";
    skyConfig.fragPath = "Shaders/Sky/sky.frag.spv";
    skyConfig.cullMode = VK_CULL_MODE_NONE;           // No culling for fullscreen quad
    skyConfig.depthTestEnable = VK_FALSE;             // Sky is always furthest
    skyConfig.depthWriteEnable = VK_FALSE;            // Don't write depth
    skyConfig.depthCompareOp = VK_COMPARE_OP_ALWAYS;  // Always pass depth test
    skyConfig.blendEnable = VK_FALSE;                 // No blending needed

    skyPipeline = MakeUnique<GraphicsPipeline>(device, skyConfig);
    skyPipeline->create();

}

void Application::featuresTestingGround() {
    std::vector<int> data = {1,2,3,4,5,6,7,8};

    BufferData bufferData(vkDevice.get());
    bufferData.setProperties(
        sizeof(int) * data.size(),

        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,

        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    bufferData.createBuffer();
    bufferData.mappedData(data);
    bufferData.unmapMemory();

    DynamicDescriptor dataDesc(vkDevice->device);
    dataDesc.createSetLayout({
        DynamicDescriptor::fastBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
    });

    dataDesc.createPool({
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}
    }, 1);

    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = dataDesc.pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &dataDesc.setLayout;

    VkDescriptorSet descSet;
    if (vkAllocateDescriptorSets(vkDevice->device, &allocInfo, &descSet) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor set");
    }

    ComputePipelineConfig computeConfig;
    computeConfig.setLayouts = {dataDesc.setLayout};
    computeConfig.compPath = "Shaders/Compute/times2.comp.spv";

    ComputePipeline computePipeline(vkDevice->device, computeConfig);
    computePipeline.create();


    // Allocate descriptor set from a pool
    VkDescriptorBufferInfo bufInfo{};
    bufInfo.buffer = bufferData.buffer;
    bufInfo.offset = 0;
    bufInfo.range  = VK_WHOLE_SIZE;

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = descSet;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufInfo;

    vkUpdateDescriptorSets(vkDevice->device, 1, &write, 0, nullptr);

    vkDevice->createCommandPool("LmaoPool", Device::GraphicsType, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // It have automatic destructor out of scope
    TemporaryCommand tempCmd(vkDevice.get(), "LmaoPool");

    vkCmdBindPipeline(tempCmd.getCmdBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.pipeline);

    vkCmdBindDescriptorSets(
        tempCmd.getCmdBuffer(),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        computePipeline.layout,
        0, 1, &descSet,
        0, nullptr
    );

    // Dispatch threads
    uint32_t numElems = static_cast<uint32_t>(data.size());
    uint32_t groupSize = 64;
    uint32_t numGroups = (numElems + groupSize - 1) / groupSize;

    vkCmdDispatch(tempCmd.getCmdBuffer(), numGroups, 1, 1);


    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = bufferData.buffer;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        tempCmd.getCmdBuffer(),
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT,
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr
    );

    tempCmd.endAndSubmit();

    void* dataPtr;
    vkMapMemory(vkDevice->device, bufferData.memory, 0, VK_WHOLE_SIZE, 0, &dataPtr);

    int* finalData = reinterpret_cast<int*>(dataPtr);

    for (size_t i = 0; i < data.size(); ++i) {
        std::cout << "Data[" << i << "] = " << finalData[i] << std::endl;
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
    opaquePipeline->setRenderPass(renderPass);
    opaquePipeline->setMsaa(newMsaaSamples);
    opaquePipeline->recreate();

    skyPipeline->setRenderPass(renderPass);
    skyPipeline->setMsaa(newMsaaSamples);
    skyPipeline->recreate();

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

}