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
    // 10km view distance for those distant horizons
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
        msaaManager->msaaSamples
    ));
    
    rasterPipeline.push_back(std::make_unique<RasterPipeline>(
        vulkanDevice->device,
        swapChain->extent,
        swapChain->imageFormat,
        "Shaders/Rasterize/raster.vert.spv",
        "Shaders/Rasterize/rasterDepth.frag.spv",
        msaaManager->msaaSamples
    ));

    rasterPipeline.push_back(std::make_unique<RasterPipeline>(
        vulkanDevice->device,
        swapChain->extent,
        swapChain->imageFormat,
        "Shaders/Rasterize/raster.vert.spv",
        "Shaders/Rasterize/rasterNormal.frag.spv",
        msaaManager->msaaSamples
    ));

    shaderManager = std::make_unique<ShaderManager>(vulkanDevice->device);

    // Create command pool for graphics operations
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = vulkanDevice->queueFamilyIndices.graphicsFamily.value();
    
    if (vkCreateCommandPool(vulkanDevice->device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }

    // Initialize render targets and depth testing
    msaaManager->createColorResources(swapChain->extent.width, swapChain->extent.height, swapChain->imageFormat);
    depthManager = std::make_unique<DepthManager>(*vulkanDevice);
    depthManager->createDepthResources(swapChain->extent.width, swapChain->extent.height, msaaManager->msaaSamples);
    swapChain->createFramebuffers(rasterPipeline[pipelineIndex]->renderPass, depthManager->depthImageView, msaaManager->colorImageView);

    buffer = std::make_unique<Buffer>(*vulkanDevice);
    buffer->createUniformBuffers(2);

    resourceManager = std::make_unique<Az3D::ResourceManager>(*vulkanDevice, commandPool);
    renderSystem = std::make_unique<Az3D::RenderSystem>();
    
    // Set up the render system to have access to materials for transparency detection
    renderSystem->setResourceManager(resourceManager.get());
    
    descriptorManager = std::make_unique<DescriptorManager>(*vulkanDevice, rasterPipeline[pipelineIndex]->descriptorSetLayout);

    // Create convenient references to avoid arrow spam
    auto& resManager = *resourceManager;
    auto& texManager = *resManager.textureManager;
    auto& meshManager = *resManager.meshManager;
    auto& matManager = *resManager.materialManager;
    auto& rendSystem = *renderSystem;

// PLAYGROUND FROM HERE!

    // Load the global pallete texture that will be used for all platformer assets
    size_t globalPaletteIndex = resManager.addTexture("GlobalPalette", "Assets/Platformer/Palette.png");
    Az3D::Material globalPaletteMaterial;
    globalPaletteMaterial.diffTxtr = globalPaletteIndex;
    size_t globalMaterialIndex = resManager.addMaterial("GlobalPalette", globalPaletteMaterial);

    // Useful shorthand function for adding platformer meshes
    std::unordered_map<std::string, size_t> platformerMeshIndices;

    auto addPlatformerMesh = [&](std::string name, std::string path) {
        std::string fullName = "Platformer/" + name;
        std::string fullPath = "Assets/Platformer/" + path;

        platformerMeshIndices[name] = resManager.addMesh(fullName, fullPath, true);
    };

    // Useful shorthand for getting a model resource index
    auto getPlatformIndex = [&](const std::string& name) {
        return rendSystem.getModelResource("Platformer/" + name);
    };
    // Useful shorthand for placing models
    auto placePlatform = [&](const std::string& name, const Az3D::Transform& transform, const glm::vec4& color = glm::vec4(1.0f)) {
        Az3D::ModelInstance instance;
        instance.modelMatrix() = transform.modelMatrix();
        instance.multColor() = color;
        instance.modelResourceIndex = getPlatformIndex(name);

        worldInstances.push_back(instance);
    };

    struct NameAndPath {
        std::string name;
        std::string path;
    };

    std::vector<NameAndPath> platformerMeshes = {
        {"Ground_x2", "ground_grass_2.obj"},
        {"Ground_x4", "ground_grass_4.obj"},
        {"Ground_x8", "ground_grass_8.obj"},
        {"Tree_1", "Tree_1.obj"},
        {"Tree_2", "Tree_2.obj"},
        {"TrailCurve_1", "trail_dirt_curved_1.obj"},
        {"TrailCurve_2", "trail_dirt_curved_2.obj"},
        {"TrailEnd_1", "trail_dirt_end_1.obj"},
        {"TrailEnd_2", "trail_dirt_end_2.obj"}
    };

    for (const auto& mesh : platformerMeshes) {
        addPlatformerMesh(mesh.name, mesh.path);
    }
    for (const auto& [name, meshIndex] : platformerMeshIndices) {
        size_t modelResourceIndex = rendSystem.addModelResource(
            "Platformer/" + name, meshIndex, globalMaterialIndex
        );
    }



    // Construct a simple world

    // Place every existing platformer model
    int count = 0;
    for (const auto& [name, meshIndex] : platformerMeshIndices) {
        Az3D::Transform transform;
        transform.pos = glm::vec3(0.0f, 0.0f, static_cast<float>(count) * 8.0f);

        placePlatform(name, transform);
        count++;
    }


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
        const auto& texture = resManager.textureManager->textures[index];
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
    printf("%s> Model:\n", WHITE);
    for (const auto& [name, index] : renderSystem->modelResourceNameToIndex) {
        const auto& modelResource = renderSystem->modelResources[index];
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
    std::vector<Az3D::Material> materialVector;
    for (const auto& matPtr : matManager.materials) {
        materialVector.push_back(*matPtr);
    }
    bufferRef.createMaterialUniformBuffers(materialVector);

    // Create descriptor sets for materials
    descManager.createDescriptorPool(2, matManager.materials.size());

    for (size_t i = 0; i < matManager.materials.size(); ++i) {
        VkBuffer materialUniformBuffer = bufferRef.getMaterialUniformBuffer(i);
        
        // Get the correct texture index from the material
        size_t textureIndex = matManager.materials[i]->diffTxtr;

        // Safety check to prevent out of bounds access
        if (textureIndex >= texManager.textures.size())
            throw std::runtime_error(
                "Material references texture index " + std::to_string(textureIndex) + 
                " but only " + std::to_string(texManager.textures.size()) + " textures are loaded!"
            );
        
        descManager.createDescriptorSetsForMaterialWithUBO(
            bufferRef.uniformBuffers, sizeof(GlobalUBO), 
            &texManager.textures[textureIndex], materialUniformBuffer, i
        );
    }

    // Load meshes into GPU buffer
    for (size_t i = 0; i < meshManager.meshes.size(); ++i) {
        bufferRef.loadMeshToBuffer(*meshManager.meshes[i]);
    }

    // Final Renderer setup with ResourceManager
    renderer = std::make_unique<Renderer>(*vulkanDevice, *swapChain, *rasterPipeline[pipelineIndex], 
                                        *buffer, *descriptorManager, *camera, *resourceManager);
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

    auto& resManager = *resourceManager;
    auto& meshManager = *resManager.meshManager;
    auto& texManager = *resManager.textureManager;
    auto& matManager = *resManager.materialManager;

    auto& rendSys = *renderSystem;

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
        rendSys.clearInstances();
        rendSys.addInstances(worldInstances);

// =================================

        // Use the new render system instead of combining model vectors
        rendererRef.drawScene(*rasterPipeline[pipelineIndex], *renderSystem);

        // On-screen FPS display (toggleable with F2) - using window title for now
        static auto lastFpsOutput = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsOutput).count() >= 500) {
            // Update FPS text every 500ms for smooth display
            std::string fpsText = "AsczGame | FPS: " + std::to_string(static_cast<int>(fpsRef.currentFPS)) +
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