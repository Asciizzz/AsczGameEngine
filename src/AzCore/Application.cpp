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
    renderSystem = std::make_unique<Az3D::RenderSystem>();
    descriptorManager = std::make_unique<DescriptorManager>(*vulkanDevice, rasterPipeline[pipelineIndex]->descriptorSetLayout);

    // Create convenient references to avoid arrow spam
    auto& texManager = *resourceManager->textureManager;
    auto& meshManager = *resourceManager->meshManager;
    auto& matManager = *resourceManager->materialManager;

// PLAYGROUND FROM HERE!

    // Load all maps (with BVH enabled for collision detection)
    size_t mapMeshIndex = meshManager.loadMeshFromOBJ("Assets/Maps/de_mirage.obj", true);
    // Az3D::Material mapMaterial;
    // mapMaterial.multColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // White color
    // mapMaterial.diffTxtr = texManager.addTexture("Assets/Textures/de_dust2.png");
    // size_t mapMaterialIndex = matManager.addMaterial(mapMaterial);
    size_t mapMaterialIndex = 0; // Use default material

    // Load all entities
    size_t sphereMeshIndex = meshManager.loadMeshFromOBJ("Assets/Shapes/Icosphere.obj");
    // Az3D::Material sphereMaterial;
    // sphereMaterial.multColor = glm::vec4(0.5f, 0.6f, 1.0f, 1.0f); // Blueish color
    // sphereMaterial.diffTxtr = texManager.addTexture("Assets/Textures/Planet.png");
    // size_t sphereMaterialIndex = matManager.addMaterial(sphereMaterial);
    size_t sphereMaterialIndex = 0; // Use default material

    // Setup map transform
    mapTransform.pos = glm::vec3(0.0f, 0.0f, 0.0f);
    mapTransform.scale(40.0f);
    // mapTransform.rotateZ(glm::radians(-45.0f));
    // mapTransform.rotateX(glm::radians(-45.0f));

    this->mapMeshIndex = mapMeshIndex;

    // Create model resources for render system
    mapModelResourceIndex = renderSystem->addModelResource(mapMeshIndex, mapMaterialIndex);
    size_t sphereModelResourceIndex = renderSystem->addModelResource(sphereMeshIndex, sphereMaterialIndex);

    particleManager.initParticles(
        100, sphereModelResourceIndex, 0.1f,
        meshManager.meshes[mapMeshIndex]->meshMin * 40.0f,
        meshManager.meshes[mapMeshIndex]->meshMax * 40.0f
    );

// PLAYGROUND END HERE 

    auto& bufferRef = *buffer;
    auto& descManager = *descriptorManager;

    // Create material uniform buffers
    std::vector<Az3D::Material> materialVector;
    for (const auto& matPtr : matManager.materials) {
        materialVector.push_back(*matPtr);
    }
    bufferRef.createMaterialUniformBuffers(materialVector);

    // Create descriptor sets for materials
    descManager.createDescriptorPool(2, matManager.materials.size());

    for (size_t i = 0; i < matManager.materials.size(); ++i) {
        VkBuffer materialUniformBuffer = bufferRef.getMaterialUniformBuffer(i);
        descManager.createDescriptorSetsForMaterialWithUBO(
            bufferRef.uniformBuffers, sizeof(GlobalUBO), 
            &texManager.textures[i], materialUniformBuffer, i
        );
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


        // Camera movement controls
        bool fast = k_state[SDL_SCANCODE_LSHIFT] && !k_state[SDL_SCANCODE_LCTRL];
        bool slow = k_state[SDL_SCANCODE_LCTRL] && !k_state[SDL_SCANCODE_LSHIFT];
        float p_speed = (fast ? 26.0f : (slow ? 0.5f : 8.0f)) * dTime;

        // Move the camera normally
        if (k_state[SDL_SCANCODE_W]) camPos += camRef.forward * p_speed;
        if (k_state[SDL_SCANCODE_S]) camPos -= camRef.forward * p_speed;
        if (k_state[SDL_SCANCODE_A]) camPos -= camRef.right * p_speed;
        if (k_state[SDL_SCANCODE_D]) camPos += camRef.right * p_speed;

        camRef.pos = camPos;

        // Clear and populate the render system for this frame
        renderSystem->clearInstances();
        
        // Add the map instance 
        Az3D::ModelInstance mapInstance;
        mapInstance.modelMatrix() = mapTransform.modelMatrix();
        mapInstance.modelResourceIndex = mapModelResourceIndex;

        renderSystem->addInstance(mapInstance);

        static bool physic_enable = false;
        static bool hold_P = false;
        if (k_state[SDL_SCANCODE_P] && !hold_P) {
            hold_P = true;

            if (k_state[SDL_SCANCODE_LSHIFT]) {
                // Reset the particle system to the camera position
                physic_enable = false; // Disable physics to setup

                for (size_t i = 0; i < particleManager.particleCount; ++i) {
                    particleManager.particles[i].pos = camRef.pos +
                        // glm::vec3(0.0f, particleManager.particleRadius * 2 * i, 0.0f);
                        glm::vec3(0.0f, 0.0f, 0.0f);

                    glm::vec3 rnd_direction = ParticleManager::randomDirection();
                    glm::vec3 mult_direction = { 5.0f, 5.0f, 5.0f };

                    particleManager.particles_velocity[i] = {
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

        particleManager.addToRenderSystem(*renderSystem);
        if (physic_enable) particleManager.update(dTime, *meshManager.meshes[mapMeshIndex], mapTransform);

        // End of particle system update

// =================================

        // Use the new render system instead of combining model vectors
        rendererRef.drawFrame(*rasterPipeline[pipelineIndex], *renderSystem, {});

        // On-screen FPS display (toggleable with F2) - using window title for now
        static auto lastFpsOutput = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsOutput).count() >= 500) {
            // Update FPS text every 500ms for smooth display
            std::string fpsText = "Az3D Engine | FPS: " + std::to_string(static_cast<int>(fpsRef.currentFPS)) +
                                    " | Avg: " + std::to_string(static_cast<int>(fpsRef.getAverageFPS())) +
                                    " | " + std::to_string(static_cast<int>(fpsRef.frameTimeMs * 10) / 10.0f) + "ms" +
                                    " | Pipeline: " + std::to_string(pipelineIndex) +
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