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
    graphicsPipelines.push_back(std::make_unique<GraphicsPipeline>(
        vulkanDevice->device,
        swapChain->extent,
        swapChain->imageFormat,
        "Shaders/hello.vert.spv",
        "Shaders/hello.frag.spv",
        msaaManager->msaaSamples
    ));
    
    graphicsPipelines.push_back(std::make_unique<GraphicsPipeline>(
        vulkanDevice->device,
        swapChain->extent,
        swapChain->imageFormat,
        "Shaders/hello.vert.spv",
        "Shaders/hello1.frag.spv",
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
    swapChain->createFramebuffers(graphicsPipelines[pipelineIndex]->renderPass, depthManager->depthImageView, msaaManager->colorImageView);

    buffer = std::make_unique<Buffer>(*vulkanDevice);
    buffer->createUniformBuffers(2);

    resourceManager = std::make_unique<Az3D::ResourceManager>(*vulkanDevice, commandPool);

// PLAYGROUND FROM HERE!

    // Create convenient references to avoid arrow spam
    auto& texManager = *resourceManager->textureManager;
    auto& meshManager = *resourceManager->meshManager;
    auto& bufferRef = *buffer;

    // Load textures and get their indices
    size_t dust2TextureIndex = resourceManager->addTexture("Model/de_dust2.png");
    size_t shirokoTextureIndex = resourceManager->addTexture("Model/Shiroko.jpg");
    size_t cubeTextureIndex = resourceManager->addTexture("old/texture1.png");

    // Create materials with texture indices
    Az3D::Material dust2Material;
    dust2Material.albedoColor = glm::vec3(1.0f, 1.0f, 1.0f);
    dust2Material.roughness = 0.7f;
    dust2Material.metallic = 0.1f;
    dust2Material.diffTxtr = dust2TextureIndex;
    size_t dust2MaterialIndex = resourceManager->addMaterial(dust2Material);

    Az3D::Material shirokoMaterial;
    shirokoMaterial.albedoColor = glm::vec3(1.0f, 1.0f, 1.0f);
    shirokoMaterial.roughness = 0.4f;
    shirokoMaterial.metallic = 0.0f;
    shirokoMaterial.diffTxtr = shirokoTextureIndex;
    size_t shirokoMaterialIndex = resourceManager->addMaterial(shirokoMaterial);

    Az3D::Material cubeMaterial;
    cubeMaterial.albedoColor = glm::vec3(0.8f, 0.9f, 1.0f);
    cubeMaterial.roughness = 0.3f;
    cubeMaterial.metallic = 0.2f;
    cubeMaterial.diffTxtr = cubeTextureIndex;
    size_t cubeMaterialIndex = resourceManager->addMaterial(cubeMaterial);

    // Load meshes and get their indices
    size_t dust2MeshIndex = meshManager.loadMeshFromOBJ("Model/de_dust2.obj");
    size_t shirokoMeshIndex = meshManager.loadMeshFromOBJ("Model/shiroko.obj");
    size_t cubeMeshIndex = meshManager.createCubeMesh();

    // Load meshes into GPU buffer
    auto dust2Mesh = meshManager.getMesh(dust2MeshIndex);
    auto shirokoMesh = meshManager.getMesh(shirokoMeshIndex);
    auto cubeMesh = meshManager.getMesh(cubeMeshIndex);

    size_t dust2BufferIndex = bufferRef.loadMeshToBuffer(*dust2Mesh);
    size_t shirokoBufferIndex = bufferRef.loadMeshToBuffer(*shirokoMesh);
    size_t cubeBufferIndex = bufferRef.loadMeshToBuffer(*cubeMesh);

    // Create descriptor sets for materials
    descriptorManager = std::make_unique<DescriptorManager>(*vulkanDevice, graphicsPipelines[pipelineIndex]->descriptorSetLayout);
    descriptorManager->createDescriptorPool(2, texManager.getTextureCount());

    auto dust2Texture = texManager.getTexture(dust2TextureIndex);
    auto shirokoTexture = texManager.getTexture(shirokoTextureIndex);
    auto cubeTexture = texManager.getTexture(cubeTextureIndex);

    auto& descManager = *descriptorManager;
    descManager.createDescriptorSetsForMaterial(bufferRef.uniformBuffers, sizeof(UniformBufferObject), dust2Texture, dust2MaterialIndex);
    descManager.createDescriptorSetsForMaterial(bufferRef.uniformBuffers, sizeof(UniformBufferObject), shirokoTexture, shirokoMaterialIndex);
    descManager.createDescriptorSetsForMaterial(bufferRef.uniformBuffers, sizeof(UniformBufferObject), cubeTexture, cubeMaterialIndex);


    // Create models using indices
    models.resize(2);

    models[0] = Az3D::Model(dust2MeshIndex, dust2MaterialIndex);
    models[0].trform.pos = glm::vec3(0.0f, .0f, 0.0f);

    models[1] = Az3D::Model(shirokoMeshIndex, shirokoMaterialIndex);
    models[1].trform.scale(0.2f);
    models[1].trform.pos = glm::vec3(0.0f, 0.0f, 0.0f);

    // Final Renderer setup with ResourceManager
    renderer = std::make_unique<Renderer>(*vulkanDevice, *swapChain, *graphicsPipelines[pipelineIndex], 
                                        *buffer, *descriptorManager, *camera, *resourceManager);
}

void Application::createSurface() {
    if (!SDL_Vulkan_CreateSurface(windowManager->window, vulkanInstance->instance, &surface)) {
        throw std::runtime_error("failed to create window surface!");
    }
}

void Application::mainLoop() {
    bool mouseLocked = true; // Track mouse lock state
    SDL_SetRelativeMouseMode(SDL_TRUE); // Start with mouse locked

    // Create references to avoid arrow spam
    auto& winManager = *windowManager;
    auto& fpsRef = *fpsManager;
    auto& camRef = *camera;
    auto& rendererRef = *renderer;
    auto& deviceRef = *vulkanDevice;

    // Get window center for mouse locking
    int windowWidth, windowHeight;
    SDL_GetWindowSize(winManager.window, &windowWidth, &windowHeight);
    int centerX = windowWidth / 2;
    int centerY = windowHeight / 2;

    int cubeCount = 0;

    float camDist = 1.0f;
    glm::vec3 camPos = camRef.position;

    while (!winManager.shouldCloseFlag) {
        // Update FPS manager for timing
        fpsRef.update();
        winManager.pollEvents();

        float dTime = fpsRef.deltaTime;

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
            swapChain->recreate(winManager.window, graphicsPipelines[pipelineIndex]->renderPass, depthManager->depthImageView, msaaManager->colorImageView);
            graphicsPipelines[pipelineIndex]->recreate(swapChain->extent, swapChain->imageFormat, depthManager->depthFormat, msaaManager->msaaSamples);
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
                SDL_WarpMouseInWindow(winManager.window, centerX, centerY);
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
            pipelineIndex = (pipelineIndex + 1) % graphicsPipelines.size();
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
        float shiro_speed = (fast ? 8.0f : (slow ? 0.5f : 3.0f)) * dTime;

        // Move the camera normally
        if (k_state[SDL_SCANCODE_W]) camPos += camRef.forward * shiro_speed;
        if (k_state[SDL_SCANCODE_S]) camPos -= camRef.forward * shiro_speed;
        if (k_state[SDL_SCANCODE_A]) camPos -= camRef.right * shiro_speed;
        if (k_state[SDL_SCANCODE_D]) camPos += camRef.right * shiro_speed;

        camRef.position = camPos;
        //*/

        /*
        auto& shiro_model = models[1];
        static float shiro_vy = 0.0f;

        // Rotate shiroko plush based on the camera's direction
        glm::vec3 s_right = glm::normalize(camera->right);
        glm::vec3 s_up = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 s_backward = glm::normalize(glm::cross(s_right, s_up));
        glm::quat s_rotation = glm::quatLookAt(s_backward, s_up);
        shiro_model.trform.rot = s_rotation;

        // 3rd person camera positioning

        // // Scroll to adjust camera distance
        if (k_state[SDL_SCANCODE_UP]) camDist -= 1.0f * dTime;
        if (k_state[SDL_SCANCODE_DOWN]) camDist += 1.0f * dTime;

        camera->position = shiro_model.trform.pos - camera->forward * camDist;

        // Move the shiroko plush based on WASD keys

        bool fast = k_state[SDL_SCANCODE_LSHIFT] && !k_state[SDL_SCANCODE_LCTRL];
        bool slow = k_state[SDL_SCANCODE_LCTRL] && !k_state[SDL_SCANCODE_LSHIFT];
        float shiro_speed = (fast ? 8.0f : (slow ? 0.5f : 3.0f)) * dTime;
        if (k_state[SDL_SCANCODE_W]) shiro_model.trform.pos -= s_backward * shiro_speed;
        if (k_state[SDL_SCANCODE_S]) shiro_model.trform.pos += s_backward * shiro_speed;
        if (k_state[SDL_SCANCODE_A]) shiro_model.trform.pos -= s_right * shiro_speed;
        if (k_state[SDL_SCANCODE_D]) shiro_model.trform.pos += s_right * shiro_speed;
        if (k_state[SDL_SCANCODE_Q]) shiro_model.trform.pos += s_up * shiro_speed;
        if (k_state[SDL_SCANCODE_E]) shiro_model.trform.pos -= s_up * shiro_speed;

        // Press space to jump, simulating gravity
        shiro_model.trform.pos.y += shiro_vy * dTime;
        if (shiro_model.trform.pos.y < 0.0f) {
            shiro_model.trform.pos.y = 0.0f; // Prevent going below ground
        } else {
            shiro_vy -= 9.81f * dTime; // Apply gravity
        }
        if (k_state[SDL_SCANCODE_SPACE] && shiro_model.trform.pos.y <= 0.0f) {
            // Simple jump logic
            shiro_vy = 3.0f; // Jump height
        }
        //*/

        // Update instance buffers dynamically by mesh type - optimized with caching + frustum culling
        std::vector<InstanceData> dust2Instances, shirokoInstances, cubeInstances;
        
        // Reserve memory to avoid reallocations during rapid spawning
        dust2Instances.reserve(models.size());
        shirokoInstances.reserve(models.size());
        cubeInstances.reserve(models.size());

        for (const auto& model : models) {
            InstanceData instanceData{};
            instanceData.modelMatrix = model.trform.modelMatrix();
            
            if (model.meshIndex == 0) {
                dust2Instances.push_back(instanceData);
            } else if (model.meshIndex == 1) {
                shirokoInstances.push_back(instanceData);
            } else if (model.meshIndex == 2) {
                cubeInstances.push_back(instanceData);
            }
        }

        // Track previous counts to avoid unnecessary buffer recreation
        static size_t prevdust2Count = 0;
        static size_t prevShirokoCount = 0;
        static size_t prevCubeCount = 0;

        // Create reference to buffer to avoid arrow spam
        auto& bufferRef = *buffer;
        auto& deviceRef = *vulkanDevice;

        // Update buffers only when count changes or just update data if count is same
        if (!dust2Instances.empty()) {
            if (dust2Instances.size() != prevdust2Count) {
                // Only wait for GPU if we're actually recreating buffers
                if (prevdust2Count > 0) vkDeviceWaitIdle(deviceRef.device);
                bufferRef.createInstanceBufferForMesh(0, dust2Instances);
                prevdust2Count = dust2Instances.size();
            } else {
                bufferRef.updateInstanceBufferForMesh(0, dust2Instances);
            }
        }
        if (!shirokoInstances.empty()) {
            if (shirokoInstances.size() != prevShirokoCount) {
                if (prevShirokoCount > 0) vkDeviceWaitIdle(deviceRef.device);
                bufferRef.createInstanceBufferForMesh(1, shirokoInstances);
                prevShirokoCount = shirokoInstances.size();
            } else {
                bufferRef.updateInstanceBufferForMesh(1, shirokoInstances);
            }
        }
        if (!cubeInstances.empty()) {
            if (cubeInstances.size() != prevCubeCount) {
                if (prevCubeCount > 0) vkDeviceWaitIdle(deviceRef.device);
                bufferRef.createInstanceBufferForMesh(2, cubeInstances);
                prevCubeCount = cubeInstances.size();
            } else {
                bufferRef.updateInstanceBufferForMesh(2, cubeInstances);
            }
        }

// =================================
        rendererRef.drawFrameWithModels(models, *graphicsPipelines[pipelineIndex]);
        
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