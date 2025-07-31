#include "AzCore/Application.hpp"

#include <iostream>
#include <random>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = false;
#endif

using namespace AzVulk;
using namespace AzBeta;

Application::Application(const char* title, uint32_t width, uint32_t height)
    : appTitle(title), appWidth(width), appHeight(height) {

    windowManager = std::make_unique<AzCore::WindowManager>(title, width, height);
    fpsManager = std::make_unique<AzCore::FpsManager>();

    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    // 10KM VIEW DISTANCE!!!
    camera = std::make_unique<Az3D::Camera>(glm::vec3(0.0f), 45.0f, 0.01f, 10000.0f);
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

    // Load all maps

    size_t mapMeshIndex = meshManager.loadMeshFromOBJ("Model/de_dust2.obj");

    // Load all entities

    size_t sphereMeshIndex = meshManager.loadMeshFromOBJ("Model/lowpoly.obj");
    Az3D::Material sphereMaterial;
    // sphereMaterial.diffTxtr = texManager.addTexture("Textures/Planet.png");
    size_t sphereMaterialIndex = matManager.addMaterial(sphereMaterial);

    // Map model

    models.push_back(Az3D::Model(mapMeshIndex, 0));
    models.back().trform.pos = glm::vec3(-20.0f, 0.0f, 0.0f);
    // models[0].trform.scale(0.1f);
    // models[0].trform.rotateZ(glm::radians(-45.0f));
    // models[0].trform.rotateX(glm::radians(-45.0f));

    gameMap.meshIndex = models[0].meshIndex;
    gameMap.trform = models[0].trform;
    gameMap.createBVH(*meshManager.meshes[gameMap.meshIndex]);

    particleManager.initParticles(100, sphereMeshIndex, sphereMaterialIndex, 0.1f);

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

    auto& meshManager = *resourceManager->meshManager;
    auto& texManager = *resourceManager->textureManager;
    auto& matManager = *resourceManager->materialManager;

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


        //*
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

        /*
        auto& p_model = models[1];
        static float p_vy = 0.0f;

        // 3rd person camera positioning

        glm::vec3 player_pos = p_model.trform.pos + glm::vec3(0.0f, 0.2f, 0.0f);

        static float current_distance = cam_dist;
        HitInfo desired_hit = gameMap.closestHit(
            *meshManager.meshes[gameMap.meshIndex],
            player_pos, -camera->forward, cam_dist);

        if (desired_hit.index == -1) {
            desired_hit.prop.z = cam_dist; // Default to cam_dist if no hit
        }

        // Avoid desired distance being too small
        // desired_distance = std::max(desired_distance, 0.5f) * 0.9f;

        current_distance += (desired_hit.prop.z - current_distance) * 10.0f * dTime;

        camera->pos = player_pos - camera->forward * desired_hit.prop.z;

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

        glm::vec3 horizon_dir = glm::vec3(0.0f);
        if (k_w) {
            horizon_dir -= s_backward;
            desired_rot = glm::quatLookAt(s_backward, s_up);
        }
        if (k_s) {
            horizon_dir += s_backward;
            desired_rot = glm::quatLookAt(-s_backward, s_up);
        }
        if (k_a) {
            horizon_dir -= s_right;
            desired_rot = glm::quatLookAt(s_right, s_up);
        }
        if (k_d) {
            horizon_dir += s_right;
            desired_rot = glm::quatLookAt(-s_right, s_up);
        }

        // Interpolate rotation towards desired rotation
        float rotationSpeed = 20.0f * dTime; // Adjust rotation speed as needed
        current_rot = glm::slerp(current_rot, desired_rot, rotationSpeed);
        p_model.trform.rot = current_rot;
        
        // Uniform scaling
        if (glm::length(horizon_dir) > 0.0f)
            horizon_dir = glm::normalize(horizon_dir);

        // Horizontal collision detection
        HitInfo collision = gameMap.closestHit(
            *meshManager.meshes[gameMap.meshIndex],
            player_pos, horizon_dir, 0.1f);

        if (collision.index == -1) {
            p_model.trform.pos += horizon_dir * p_speed;
        } else {
            // Shoot the player back based on the collision normal
            glm::vec3 collision_normal = collision.nrml;

            p_model.trform.pos -= collision_normal * glm::dot(horizon_dir, collision_normal);
        }

        // // Press space to jump, simulating gravity
        // p_model.trform.pos.y += p_vy * dTime;
        // if (p_model.trform.pos.y < 0.0f) {
        //     p_model.trform.pos.y = 0.0f; // Prevent going below ground
        // } else {
        //     p_vy -= 9.81f * dTime; // Apply gravity
        // }

        glm::vec3 downward_dir = glm::vec3(0.0f, -1.0f, 0.0f);
        static float velocityY = 0.0f;
        velocityY -= 9.81f * dTime; // Gravity effect
        velocityY = std::max(velocityY, -10.0f); // Terminal velocity (kind of)

        if (k_state[SDL_SCANCODE_SPACE]) {
            velocityY = 3.0f;
        }

        HitInfo groundHit = gameMap.closestHit(
            *meshManager.meshes[gameMap.meshIndex],
            player_pos, downward_dir, 0.2f);
        
        // No collision
        if (groundHit.index == -1 || velocityY > 0.0f) {
            p_model.trform.pos.y += velocityY * dTime;
        } else {
            velocityY = 0.0f;
            p_model.trform.pos.y = groundHit.prop.z;
        }

        if (p_model.trform.pos.y < -10.0f) {
            p_model.trform.pos.y = 0.0f; // Reset position if too low
            velocityY = 0.0f; // Reset vertical velocity
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
        // Update instance buffers for the particle system
        std::vector<ModelInstance> particleInstances;
        particleInstances.reserve(particleManager.particleCount);
        for (const auto& particle : particleManager.particles) {
            ModelInstance instance{};
            instance.modelMatrix = particle.trform.modelMatrix();

            particleInstances.push_back(instance);
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
        // Update particle instance buffer
        static size_t prevParticleCount = 0;

        size_t particleMeshIndex = particleManager.meshIndex;
        if (particleInstances.size() != prevParticleCount) {
            if (prevParticleCount > 0) vkDeviceWaitIdle(deviceRef.device);
            bufferRef.createInstanceBufferForMesh(particleMeshIndex, particleInstances);
            prevParticleCount = particleInstances.size();
        } else {
            bufferRef.updateInstanceBufferForMesh(particleMeshIndex, particleInstances);
        }

        printf("Particle count: %zu\n", particleManager.particleCount);

        static bool physic_enable = false;
        static bool hold_P = false;
        if (k_state[SDL_SCANCODE_P] && !hold_P) {
            hold_P = true;

            if (k_state[SDL_SCANCODE_LSHIFT]) {
                // Reset the particle system to the camera position
                physic_enable = false; // Disable physics to setup

                for (size_t i = 0; i < particleManager.particleCount; ++i) {
                    particleManager.particles[i].trform.pos = camRef.pos;

                    glm::vec3 rnd_direction = ParticleManager::randomDirection();
                    glm::vec3 mult_direction = { 4.0f, 4.0f, 4.0f };

                    particleManager.particles_direction[i] = {
                        rnd_direction.x * mult_direction.x,
                        rnd_direction.y * mult_direction.y,
                        rnd_direction.z * mult_direction.z
                    };
                }

            } else {
                physic_enable = !physic_enable;
            }

        } else if (!k_state[SDL_SCANCODE_P]) {
            hold_P = false;
        }

        if (physic_enable)
            particleManager.update(dTime, *meshManager.meshes[gameMap.meshIndex], gameMap);

        //*/


// =================================

        // Combine all models into a single vector for rendering
        std::vector<Az3D::Model> allModels = models;
        // Combine particle.particles into the same vector
        allModels.insert(allModels.end(), particleManager.particles.begin(), particleManager.particles.end());

        rendererRef.drawFrame(*rasterPipeline[pipelineIndex], allModels, {});

        // On-screen FPS display (toggleable with F2) - using window title for now
        static auto lastFpsOutput = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsOutput).count() >= 500) {
            // Update FPS text every 500ms for smooth display
            std::string fpsText = "Az3D Engine | FPS: " + std::to_string(static_cast<int>(fpsRef.currentFPS)) +
                                    " | Avg: " + std::to_string(static_cast<int>(fpsRef.getAverageFPS())) +
                                    " | " + std::to_string(static_cast<int>(fpsRef.frameTimeMs * 10) / 10.0f) + "ms" +
                                    " | Pipeline: " + std::to_string(pipelineIndex) +
                                    " | Pos: " + std::to_string(camRef.pos.x) + ", " +
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