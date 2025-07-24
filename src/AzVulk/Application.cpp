#include "AzVulk/Application.hpp"
#include "Az3D/Az3D.hpp"
#include <SDL2/SDL_vulkan.h>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

namespace AzVulk {
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
        vulkanInstance = std::make_unique<VulkanInstance>(extensions, enableValidationLayers);
        createSurface();

        vulkanDevice = std::make_unique<VulkanDevice>(vulkanInstance->instance, surface);
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

        // Load textures and get their indices
        size_t vikingTextureIndex = resourceManager->addTexture("Model/viking_room.png");
        size_t shirokoTextureIndex = resourceManager->addTexture("Model/Shiroko.jpg");
        size_t cubeTextureIndex = resourceManager->addTexture("textures/texture1.png");

        // Create materials with texture indices
        Az3D::Material vikingMaterial;
        vikingMaterial.albedoColor = glm::vec3(1.0f, 1.0f, 1.0f);
        vikingMaterial.roughness = 0.7f;
        vikingMaterial.metallic = 0.1f;
        vikingMaterial.diffTxtr = vikingTextureIndex;
        size_t vikingMaterialIndex = resourceManager->addMaterial(vikingMaterial);

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
        size_t vikingMeshIndex = resourceManager->meshManager->loadMeshFromOBJ("Model/viking_room.obj");
        size_t shirokoMeshIndex = resourceManager->meshManager->loadMeshFromOBJ("Model/shiroko.obj");
        size_t cubeMeshIndex = resourceManager->meshManager->createCubeMesh();;

        // Load meshes into GPU buffer
        std::cout << "\n[GPU BUFFER LOADING] Loading meshes to GPU..." << std::endl;
        auto vikingMesh = resourceManager->meshManager->getMesh(vikingMeshIndex);
        auto shirokoMesh = resourceManager->meshManager->getMesh(shirokoMeshIndex);
        auto cubeMesh = resourceManager->meshManager->getMesh(cubeMeshIndex);

        size_t vikingBufferIndex = buffer->loadMeshToBuffer(*vikingMesh);
        size_t shirokoBufferIndex = buffer->loadMeshToBuffer(*shirokoMesh);
        size_t cubeBufferIndex = buffer->loadMeshToBuffer(*cubeMesh);

        // Create descriptor sets for materials
        descriptorManager = std::make_unique<DescriptorManager>(*vulkanDevice, graphicsPipelines[pipelineIndex]->descriptorSetLayout);
        descriptorManager->createDescriptorPool(2, resourceManager->textureManager->getTextureCount());
        
        std::cout << "\n[DESCRIPTOR CREATION] Creating descriptor sets..." << std::endl;
        auto vikingTexture = resourceManager->textureManager->getTexture(vikingTextureIndex);
        auto shirokoTexture = resourceManager->textureManager->getTexture(shirokoTextureIndex);
        auto cubeTexture = resourceManager->textureManager->getTexture(cubeTextureIndex);
        
        if (vikingTexture) {
            descriptorManager->createDescriptorSetsForMaterial(buffer->uniformBuffers, sizeof(UniformBufferObject), vikingTexture, vikingMaterialIndex);
            std::cout << "  ✓ Created descriptor set for viking material (index " << vikingMaterialIndex << ")" << std::endl;
        }
        if (shirokoTexture) {
            descriptorManager->createDescriptorSetsForMaterial(buffer->uniformBuffers, sizeof(UniformBufferObject), shirokoTexture, shirokoMaterialIndex);
            std::cout << "  ✓ Created descriptor set for shiroko material (index " << shirokoMaterialIndex << ")" << std::endl;
        }
        if (cubeTexture) {
            descriptorManager->createDescriptorSetsForMaterial(buffer->uniformBuffers, sizeof(UniformBufferObject), cubeTexture, cubeMaterialIndex);
            std::cout << "  ✓ Created descriptor set for cube material (index " << cubeMaterialIndex << ")" << std::endl;
        }

        // Create models using indices
        models.resize(2);

        models[0] = Az3D::Model(vikingMeshIndex, vikingMaterialIndex);
        models[0].trform.position = glm::vec3(0.0f, .0f, 0.0f);
        models[0].trform.rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

        models[1] = Az3D::Model(shirokoMeshIndex, shirokoMaterialIndex);
        models[1].trform.scale(0.2f);
        models[1].trform.position = glm::vec3(0.0f, 0.0f, 0.0f);

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
        bool showFPS = true; // FPS display toggle state

        // Get window center for mouse locking
        int windowWidth, windowHeight;
        SDL_GetWindowSize(windowManager->window, &windowWidth, &windowHeight);
        int centerX = windowWidth / 2;
        int centerY = windowHeight / 2;

        int cubeCount = 0;

        float camDist = 1.0f;
        glm::vec3 camPos = camera->position;

        while (!windowManager->shouldCloseFlag) {
            // Update FPS manager for timing
            fpsManager->update();
            windowManager->pollEvents();

            float dTime = fpsManager->deltaTime;

            // Check if window was resized or renderer needs to be updated
            if (windowManager->resizedFlag || renderer->framebufferResized) {
                windowManager->resizedFlag = false;
                renderer->framebufferResized = false;

                vkDeviceWaitIdle(vulkanDevice->device);

                int newWidth, newHeight;
                SDL_GetWindowSize(windowManager->window, &newWidth, &newHeight);

                // Reset like literally everything
                camera->updateAspectRatio(newWidth, newHeight);
                msaaManager->createColorResources(newWidth, newHeight, swapChain->imageFormat);
                depthManager->createDepthResources(newWidth, newHeight, msaaManager->msaaSamples);
                swapChain->recreate(windowManager->window, graphicsPipelines[pipelineIndex]->renderPass, depthManager->depthImageView, msaaManager->colorImageView);
                graphicsPipelines[pipelineIndex]->recreate(swapChain->extent, swapChain->imageFormat, depthManager->depthFormat, msaaManager->msaaSamples);
            }

            const Uint8* k_state = SDL_GetKeyboardState(nullptr);
            if (k_state[SDL_SCANCODE_ESCAPE]) {
                windowManager->shouldCloseFlag = true;
                break;
            }
            
            // Toggle mouse lock with F1 key
            static bool f1Pressed = false;
            if (k_state[SDL_SCANCODE_F1] && !f1Pressed) {
                mouseLocked = !mouseLocked;
                if (mouseLocked) {
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    SDL_WarpMouseInWindow(windowManager->window, centerX, centerY);
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
            
            // Toggle FPS display with F2 key
            static bool f2Pressed = false;
            if (k_state[SDL_SCANCODE_F2] && !f2Pressed) {
                showFPS = !showFPS;
                std::cout << "FPS display: " << (showFPS ? "ON" : "OFF") << std::endl;
                f2Pressed = true;
            } else if (!k_state[SDL_SCANCODE_F2]) {
                f2Pressed = false;
            }

            // Handle mouse look when locked
            if (mouseLocked) {
                int mouseX, mouseY;
                SDL_GetRelativeMouseState(&mouseX, &mouseY);

                float sensitivity = 0.02f;
                float yawDelta = mouseX * sensitivity;
                float pitchDelta = -mouseY * sensitivity;

                camera->rotate(pitchDelta, yawDelta, 0.0f);
            }
        
    // ======== PLAYGROUND HERE! ========


            bool fast = k_state[SDL_SCANCODE_LSHIFT] && !k_state[SDL_SCANCODE_LCTRL];
            bool slow = k_state[SDL_SCANCODE_LCTRL] && !k_state[SDL_SCANCODE_LSHIFT];
            float shiro_speed = (fast ? 8.0f : (slow ? 0.5f : 3.0f)) * dTime;

            // Move the camer normally
            if (k_state[SDL_SCANCODE_W]) camPos += camera->forward * shiro_speed;
            if (k_state[SDL_SCANCODE_S]) camPos -= camera->forward * shiro_speed;
            if (k_state[SDL_SCANCODE_A]) camPos -= camera->right * shiro_speed;
            if (k_state[SDL_SCANCODE_D]) camPos += camera->right * shiro_speed;

            camera->position = camPos;


            // Update instance buffers dynamically by mesh type - optimized with caching + frustum culling
            std::vector<InstanceData> vikingInstances, shirokoInstances, cubeInstances;
            
            // Reserve memory to avoid reallocations during rapid spawning
            vikingInstances.reserve(models.size());
            shirokoInstances.reserve(models.size());
            cubeInstances.reserve(models.size());

            for (const auto& model : models) {
                InstanceData instanceData{};
                instanceData.modelMatrix = model.trform.modelMatrix();
                
                if (model.meshIndex == 0) {
                    vikingInstances.push_back(instanceData);
                } else if (model.meshIndex == 1) {
                    shirokoInstances.push_back(instanceData);
                } else if (model.meshIndex == 2) {
                    cubeInstances.push_back(instanceData);
                }
            }

            // Track previous counts to avoid unnecessary buffer recreation
            static size_t prevVikingCount = 0;
            static size_t prevShirokoCount = 0;
            static size_t prevCubeCount = 0;
            
            // Update buffers only when count changes or just update data if count is same
            if (!vikingInstances.empty()) {
                if (vikingInstances.size() != prevVikingCount) {
                    // Only wait for GPU if we're actually recreating buffers
                    if (prevVikingCount > 0) vkDeviceWaitIdle(vulkanDevice->device);
                    buffer->createInstanceBufferForMesh(0, vikingInstances);
                    prevVikingCount = vikingInstances.size();
                } else {
                    buffer->updateInstanceBufferForMesh(0, vikingInstances);
                }
            }
            if (!shirokoInstances.empty()) {
                if (shirokoInstances.size() != prevShirokoCount) {
                    if (prevShirokoCount > 0) vkDeviceWaitIdle(vulkanDevice->device);
                    buffer->createInstanceBufferForMesh(1, shirokoInstances);
                    prevShirokoCount = shirokoInstances.size();
                } else {
                    buffer->updateInstanceBufferForMesh(1, shirokoInstances);
                }
            }
            if (!cubeInstances.empty()) {
                if (cubeInstances.size() != prevCubeCount) {
                    if (prevCubeCount > 0) vkDeviceWaitIdle(vulkanDevice->device);
                    buffer->createInstanceBufferForMesh(2, cubeInstances);
                    prevCubeCount = cubeInstances.size();
                } else {
                    buffer->updateInstanceBufferForMesh(2, cubeInstances);
                }
            }

    // =================================
            renderer->drawFrameWithModels(models, *graphicsPipelines[pipelineIndex]);
            
            // On-screen FPS display (toggleable with F2) - using window title for now
            static auto lastFpsOutput = std::chrono::steady_clock::now();
            static bool lastShowFPS = showFPS;
            auto now = std::chrono::steady_clock::now();
            
            if (showFPS && std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsOutput).count() >= 500) {
                // Update FPS text every 500ms for smooth display
                std::string fpsText = "Az3D Engine | FPS: " + std::to_string(static_cast<int>(fpsManager->currentFPS)) +
                                     " | Avg: " + std::to_string(static_cast<int>(fpsManager->getAverageFPS())) +
                                     " | " + std::to_string(static_cast<int>(fpsManager->frameTimeMs * 10) / 10.0f) + "ms" +
                                     " | Pipeline: " + std::to_string(pipelineIndex);
                SDL_SetWindowTitle(windowManager->window, fpsText.c_str());
                lastFpsOutput = now;
            } else if (!showFPS && lastShowFPS) {
                // Reset to default title when FPS display is turned off
                SDL_SetWindowTitle(windowManager->window, "Az3D Engine");
            }
            lastShowFPS = showFPS;
        }

        vkDeviceWaitIdle(vulkanDevice->device);
    }

    void Application::cleanup() {
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(vulkanDevice->device, commandPool, nullptr);
        }
        
        if (surface != VK_NULL_HANDLE && vulkanInstance) {
            vkDestroySurfaceKHR(vulkanInstance->instance, surface, nullptr);
        }
    }
}
