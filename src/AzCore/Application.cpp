#include "AzCore/Application.hpp"

#include <iostream>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

using namespace AzVulk;

Application::Application(const char* title, uint32_t width, uint32_t height)
    : appTitle(title), appWidth(width), appHeight(height) {

    windowManager = std::make_unique<AzCore::WindowManager>(title, width, height);
    fpsManager = std::make_unique<AzCore::FpsManager>();

    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    camera = std::make_unique<Az3D::Camera>(glm::vec3(0.0f, 0.0f, 3.0f), 45.0f, 0.1f, 200.0f);
    camera->setAspectRatio(aspectRatio);

    initVulkan();
}

Application::~Application() {
    cleanup();
}

void Application::run() {
    mainLoop();

    std::cout << "Application exited successfully. See you next time!" << std::endl;
}

void Application::initVulkan() {
    auto extensions = windowManager->getRequiredVulkanExtensions();
    vulkanInstance = std::make_unique<Instance>(extensions, enableValidationLayers);
    createSurface();

    vulkanDevice = std::make_unique<Device>(vulkanInstance->instance, surface);
    msaaManager = std::make_unique<MSAAManager>(*vulkanDevice);
    swapChain = std::make_unique<SwapChain>(*vulkanDevice, surface, windowManager->window);

    // Create multiple graphics pipelines for different rendering modes
    rasterPipeline.push_back(std::make_unique<RasterPipeline>(
        vulkanDevice->device,
        swapChain->extent,
        swapChain->imageFormat,
        "Shaders/Rasterize/raster.vert.spv",
        "Shaders/Rasterize/raster.frag.spv",
        "Shaders/Billboard/billboard.vert.spv",
        "Shaders/Billboard/billboard.frag.spv",
        msaaManager->msaaSamples
    ));
    
    rasterPipeline.push_back(std::make_unique<RasterPipeline>(
        vulkanDevice->device,
        swapChain->extent,
        swapChain->imageFormat,
        "Shaders/Rasterize/raster.vert.spv",
        "Shaders/Rasterize/raster1.frag.spv",
        "Shaders/Billboard/billboard.vert.spv",
        "Shaders/Billboard/billboard.frag.spv",
        msaaManager->msaaSamples
    ));

    shaderManager = std::make_unique<ShaderManager>(vulkanDevice->device);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = vulkanDevice->queueFamilyIndices.graphicsFamily.value();
    
    if (vkCreateCommandPool(vulkanDevice->device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }

    msaaManager->createColorResources(swapChain->extent.width, swapChain->extent.height, swapChain->imageFormat);
    depthManager = std::make_unique<DepthManager>(*vulkanDevice);
    depthManager->createDepthResources(swapChain->extent.width, swapChain->extent.height, msaaManager->msaaSamples);
    swapChain->createFramebuffers(rasterPipeline[pipelineIndex]->renderPass, depthManager->depthImageView, msaaManager->colorImageView);

    buffer = std::make_unique<Buffer>(*vulkanDevice);
    buffer->createUniformBuffers(2);

    resourceManager = std::make_unique<Az3D::ResourceManager>(*vulkanDevice, commandPool);
    descriptorManager = std::make_unique<DescriptorManager>(*vulkanDevice, rasterPipeline[pipelineIndex]->descriptorSetLayout);

    // Create convenient references to avoid arrow spam
    auto& texManager = *resourceManager->textureManager;
    auto& meshManager = *resourceManager->meshManager;
    auto& matManager = *resourceManager->materialManager;

// PLAYGROUND FROM HERE!

    // Load textures and get their indices
    size_t mapTextureIndex = texManager.addTexture("Model/viking_room.png");
    size_t playerTextureIndex = texManager.addTexture("Model/Selen.png");

    size_t demoTextureIndex = texManager.addTexture("Model/Untitled.png");

    // Load meshes and get their indices
    size_t mapMeshIndex = meshManager.loadMeshFromOBJ("Model/viking_room.obj");
    size_t playerMeshIndex = meshManager.loadMeshFromOBJ("Model/Selen.obj");
    
    size_t partMeshIndex = meshManager.loadMeshFromOBJ("Model/Part.obj");


    // Create materials with texture indices
    Az3D::Material mapMaterial;
    mapMaterial.albedoColor = glm::vec3(1.0f, 1.0f, 1.0f);
    mapMaterial.roughness = 0.7f;
    mapMaterial.metallic = 0.1f;
    mapMaterial.diffTxtr = mapTextureIndex;
    size_t mapMaterialIndex = matManager.addMaterial(mapMaterial);

    Az3D::Material playerMaterial;
    playerMaterial.albedoColor = glm::vec3(1.0f, 1.0f, 1.0f);
    playerMaterial.roughness = 0.4f;
    playerMaterial.metallic = 0.0f;
    playerMaterial.diffTxtr = playerTextureIndex;
    size_t playerMaterialIndex = resourceManager->addMaterial(playerMaterial);


    // Create models using indices
    models.resize(2);

    models[0] = Az3D::Model(mapMeshIndex, mapMaterialIndex);
    models[0].trform.pos = glm::vec3(0.0f, .0f, 0.0f);
    models[0].trform.rotateX(glm::radians(-90.0f));

    models[1] = Az3D::Model(playerMeshIndex, playerMaterialIndex);
    models[1].trform.scale(0.1f);
    models[1].trform.pos = glm::vec3(0.0f, 0.0f, 0.0f);

    // Create billboards for testing
    billboards.resize(1);
    billboards[0] = Az3D::Billboard(glm::vec3(0), 0.12f, 0.12f, demoTextureIndex, 0.5f);

// PLAYGROUND END HERE 

    auto& bufferRef = *buffer;
    auto& descManager = *descriptorManager;

    // Create descriptor sets for materials
    descManager.createDescriptorPool(2, matManager.materials.size());

    for (size_t i = 0; i < matManager.materials.size(); ++i) {
        descManager.createDescriptorSetsForMaterial(bufferRef.uniformBuffers, sizeof(GlobalUBO), 
                                                    &texManager.textures[i], i);
    }

    // Load meshes into GPU buffer
    for (size_t i = 0; i < meshManager.meshes.size(); ++i) {
        bufferRef.loadMeshToBuffer(*meshManager.meshes[i]);
    }

    // Final Renderer setup with ResourceManager
    renderer = std::make_unique<Renderer>(*vulkanDevice, *swapChain, *rasterPipeline[pipelineIndex], 
                                        *buffer, *descriptorManager, *camera, *resourceManager);
    renderer->setupBillboardDescriptors();
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

    while (!winManager.shouldCloseFlag) {
        // Update FPS manager for timing
        fpsRef.update();
        winManager.pollEvents();

        float dTime = fpsRef.deltaTime;

        static float camDist = 1.5f;
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
            swapChain->recreate(winManager.window, rasterPipeline[pipelineIndex]->renderPass, depthManager->depthImageView, msaaManager->colorImageView);
            rasterPipeline[pipelineIndex]->recreate(swapChain->extent, swapChain->imageFormat, depthManager->depthFormat, msaaManager->msaaSamples);
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
                // Exit fullscreen
                SDL_SetWindowFullscreen(winManager.window, 0);
            } else {
                // Enter fullscreen (borderless windowed)
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
        
        // Switch graphics pipelines with Tab key
        static bool tabPressed = false;
        if (k_state[SDL_SCANCODE_TAB] && !tabPressed) {
            pipelineIndex = (pipelineIndex + 1) % rasterPipeline.size();
            tabPressed = true;
        } else if (!k_state[SDL_SCANCODE_TAB]) {
            tabPressed = false;
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


        /*
        bool fast = k_state[SDL_SCANCODE_LSHIFT] && !k_state[SDL_SCANCODE_LCTRL];
        bool slow = k_state[SDL_SCANCODE_LCTRL] && !k_state[SDL_SCANCODE_LSHIFT];
        float p_speed = (fast ? 26.0f : (slow ? 0.5f : 8.0f)) * dTime;

        // Move the camera normally
        if (k_state[SDL_SCANCODE_W]) camPos += camRef.forward * p_speed;
        if (k_state[SDL_SCANCODE_S]) camPos -= camRef.forward * p_speed;
        if (k_state[SDL_SCANCODE_A]) camPos -= camRef.right * p_speed;
        if (k_state[SDL_SCANCODE_D]) camPos += camRef.right * p_speed;

        camRef.pos = camPos;
        //*/

        //*
        auto& p_model = models[1];
        static float p_vy = 0.0f;

        // 3rd person camera positioning

        glm::vec3 player_pos = p_model.trform.pos + glm::vec3(0.0f, 0.25f, 0.0f);
        glm::vec3 desired_pos = player_pos - camera->forward * camDist;

        float ground_threshold = 0.1f;

        if (desired_pos.y < ground_threshold) {
            float dist_y = player_pos.y - desired_pos.y;

            float t = (player_pos.y - ground_threshold) / dist_y;

            desired_pos = player_pos - camera->forward * camDist * t;
        }

        // Smoothly ease the camera position towards the desired position
        // camera->pos = glm::mix(camera->pos, desired_pos, 30.0f * dTime);
        camera->pos = desired_pos;

        // Move the player plush based on WASD keys

        glm::vec3 s_right = glm::normalize(camera->right);
        glm::vec3 s_up = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 s_backward = glm::normalize(glm::cross(s_right, s_up));

        bool k_w = k_state[SDL_SCANCODE_W];
        bool k_s = k_state[SDL_SCANCODE_S];
        bool k_a = k_state[SDL_SCANCODE_A];
        bool k_d = k_state[SDL_SCANCODE_D];
        bool k_q = k_state[SDL_SCANCODE_Q];
        bool k_e = k_state[SDL_SCANCODE_E];

        bool fast = k_state[SDL_SCANCODE_LSHIFT] && !k_state[SDL_SCANCODE_LCTRL];
        bool slow = k_state[SDL_SCANCODE_LCTRL] && !k_state[SDL_SCANCODE_LSHIFT];
        float p_speed = (fast ? 8.0f : (slow ? 0.5f : 3.0f)) * dTime;

        static glm::quat desired_rot = glm::quatLookAt(s_backward, s_up);
        static glm::quat current_rot = p_model.trform.rot;

        if (k_w) {
            p_model.trform.pos -= s_backward * p_speed;
            desired_rot = glm::quatLookAt(s_backward, s_up);
        }
        if (k_s) {
            p_model.trform.pos += s_backward * p_speed;
            desired_rot = glm::quatLookAt(-s_backward, s_up);
        }
        if (k_a) {
            p_model.trform.pos -= s_right * p_speed;
            desired_rot = glm::quatLookAt(s_right, s_up);
        }
        if (k_d) {
            p_model.trform.pos += s_right * p_speed;
            desired_rot = glm::quatLookAt(-s_right, s_up);
        }

        // Interpolate rotation towards desired rotation
        float rotationSpeed = 20.0f * dTime; // Adjust rotation speed as needed
        current_rot = glm::slerp(current_rot, desired_rot, rotationSpeed);

        p_model.trform.rot = current_rot;

        // Press space to jump, simulating gravity
        p_model.trform.pos.y += p_vy * dTime;
        if (p_model.trform.pos.y < 0.0f) {
            p_model.trform.pos.y = 0.0f; // Prevent going below ground
        } else {
            p_vy -= 9.81f * dTime; // Apply gravity
        }
        if (k_state[SDL_SCANCODE_SPACE] && p_model.trform.pos.y <= 0.0f) {
            // Simple jump logic
            p_vy = 3.0f; // Jump height
        }
        //*/

        // Update instance buffers dynamically by mesh type
        size_t meshCount = resourceManager->meshManager->meshes.size();

        std::vector<std::vector<ModelInstance>> instances(meshCount);

        for (const auto& model : models) {
            ModelInstance instance{};
            instance.modelMatrix = model.trform.modelMatrix();

            instances[model.meshIndex].push_back(instance);
        }

        // Track previous counts to avoid unnecessary buffer recreation
        static std::vector<size_t> prevCounts(meshCount, 0);

        for (size_t i = 0; i < instances.size(); ++i) {
            if (instances[i].empty()) continue;

            if (instances[i].size() != prevCounts[i]) {
                if (prevCounts[i] > 0) vkDeviceWaitIdle(deviceRef.device);
                bufferRef.createInstanceBufferForMesh(i, instances[i]);
                prevCounts[i] = instances[i].size();
            } else {
                bufferRef.updateInstanceBufferForMesh(i, instances[i]);
            }
        }

        // Billboard manipulation

        static int frame = 0;
        static int frame_max = 2;
        static int frame_time = 0;
        static int frame_time_max = 1000;
        frame_time++;
        frame += frame_time > frame_time_max;
        frame -= frame * (frame > frame_max - 1);
        frame_time -= frame_time * (frame_time > frame_time_max);

        switch (frame) {
            case 0:
                billboards[0].uvMin = glm::vec2(0.0f, 0.0f);
                billboards[0].uvMax = glm::vec2(0.5f, 1.0f);
                break;
            case 1:
                billboards[0].uvMin = glm::vec2(0.5f, 0.0f);
                billboards[0].uvMax = glm::vec2(1.0f, 1.0f);
                break;
        }

        billboards[0].pos = p_model.trform.pos
            + glm::vec3(0.0f, 0.35f, 0.0f); // Position above the player



// =================================

        rendererRef.drawFrame(*rasterPipeline[pipelineIndex], models, billboards);

        // On-screen FPS display (toggleable with F2) - using window title for now
        static auto lastFpsOutput = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsOutput).count() >= 500) {
            // Update FPS text every 500ms for smooth display
            std::string fpsText = "Az3D Engine | FPS: " + std::to_string(static_cast<int>(fpsRef.currentFPS)) +
                                    " | Avg: " + std::to_string(static_cast<int>(fpsRef.getAverageFPS())) +
                                    " | " + std::to_string(static_cast<int>(fpsRef.frameTimeMs * 10) / 10.0f) + "ms" +
                                    " | Pipeline: " + std::to_string(pipelineIndex);
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