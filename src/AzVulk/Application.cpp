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
        camera = std::make_unique<AzCore::Camera>(glm::vec3(0.0f, 0.0f, 0.0f), 45.0f, 0.1f, 200.0f);
        camera->setAspectRatio(aspectRatio);

        initVulkan();
    }

    Application::~Application() {
        cleanup();
    }

    void Application::run() {
        mainLoop();
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

        // Initialize Az3D resource management system
        resourceManager = std::make_unique<Az3D::ResourceManager>(*vulkanDevice, commandPool);

        // Load meshes using the new system
        resourceManager->loadMesh("viking_room", "Model/viking_room.obj");
        resourceManager->loadMesh("shiroko", "Model/shiroko.obj");

        // Load textures using the new system
        resourceManager->loadTexture("viking_room_diffuse", "Model/viking_room.png");
        resourceManager->loadTexture("shiroko_diffuse", "Model/Shiroko.jpg");

        // Create materials using the new system
        auto vikingMaterial = resourceManager->createMaterial("viking_material", "VikingRoom");
        if (vikingMaterial) {
            vikingMaterial->setDiffuseTexture("viking_room_diffuse");
            vikingMaterial->getProperties().roughness = 0.7f;
            vikingMaterial->getProperties().metallic = 0.1f;
            vikingMaterial->getProperties().albedoColor = glm::vec3(1.0f, 1.0f, 1.0f);
        }

        auto shirokoMaterial = resourceManager->createMaterial("shiroko_material", "Shiroko");
        if (shirokoMaterial) {
            shirokoMaterial->setDiffuseTexture("shiroko_diffuse");
            shirokoMaterial->getProperties().roughness = 0.4f;
            shirokoMaterial->getProperties().metallic = 0.0f;
            shirokoMaterial->getProperties().albedoColor = glm::vec3(1.0f, 1.0f, 1.0f);
        }

        // Load meshes into buffer (using the resource manager)
        auto vikingMesh = resourceManager->getMeshManager().getMesh("viking_room");
        auto shirokoMesh = resourceManager->getMeshManager().getMesh("shiroko");
        if (vikingMesh) buffer->loadMeshToBuffer(*vikingMesh);
        if (shirokoMesh) buffer->loadMeshToBuffer(*shirokoMesh);

        // Create models using the new ID-based system
        models.resize(2);

        models[0] = Az3D::Model("viking_room", "viking_material");
        models[0].position = glm::vec3(0.0f, 0.0f, 0.0f);
        models[0].rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

        models[1] = Az3D::Model("shiroko", "shiroko_material");
        models[1].position = glm::vec3(5.0f, 0.0f, 0.0f);
        models[1].rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

        std::cout << "Created material '" << (vikingMaterial ? vikingMaterial->getName() : "null")
                  << "' with roughness=" << (vikingMaterial ? vikingMaterial->getProperties().roughness : 0.0f)
                  << ", metallic=" << (vikingMaterial ? vikingMaterial->getProperties().metallic : 0.0f)
                  << ", diffuse texture='" << (vikingMaterial ? vikingMaterial->getDiffuseTexture() : "") << "'" << std::endl;
                  
        std::cout << "Created material '" << (shirokoMaterial ? shirokoMaterial->getName() : "null")
                  << "' with roughness=" << (shirokoMaterial ? shirokoMaterial->getProperties().roughness : 0.0f)
                  << ", metallic=" << (shirokoMaterial ? shirokoMaterial->getProperties().metallic : 0.0f)
                  << ", diffuse texture='" << (shirokoMaterial ? shirokoMaterial->getDiffuseTexture() : "") << "'" << std::endl;

        // Create instance buffers for both models
        std::vector<InstanceData> instances0, instances1;
        
        InstanceData instanceData0{};
        instanceData0.modelMatrix = models[0].getModelMatrix();
        instances0.push_back(instanceData0);
        
        InstanceData instanceData1{};
        instanceData1.modelMatrix = models[1].getModelMatrix();
        instances1.push_back(instanceData1);

        buffer->createInstanceBufferForMesh(0, instances0);
        buffer->createInstanceBufferForMesh(1, instances1);

        // Create descriptor manager with Az3D texture system
        descriptorManager = std::make_unique<DescriptorManager>(*vulkanDevice, graphicsPipelines[pipelineIndex]->descriptorSetLayout);
        descriptorManager->createDescriptorPool(2); // MAX_FRAMES_IN_FLIGHT
        
        // Use Az3D texture system instead of legacy
        auto texture = resourceManager->getTextureManager().getTexture("shiroko_diffuse");
        if (texture && texture->getData()) {
            descriptorManager->createDescriptorSets(buffer->uniformBuffers, sizeof(UniformBufferObject),
                                                    texture->getData()->view, texture->getData()->sampler);
        }

        
        // Create final renderer
        renderer = std::make_unique<Renderer>(*vulkanDevice, *swapChain, *graphicsPipelines[pipelineIndex], *buffer, *descriptorManager, *camera);
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
                std::cout << "Switched to pipeline " << pipelineIndex << std::endl;
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

            bool fast = k_state[SDL_SCANCODE_LSHIFT] && !k_state[SDL_SCANCODE_LCTRL];
            bool slow = k_state[SDL_SCANCODE_LCTRL] && !k_state[SDL_SCANCODE_LSHIFT];
            float speed = fast ? 15.0f : (slow ? 2.0f : 8.0f);
            if (k_state[SDL_SCANCODE_W])
                camera->translate(camera->forward * speed * dTime);
            if (k_state[SDL_SCANCODE_S])
                camera->translate(-camera->forward * speed * dTime);
            if (k_state[SDL_SCANCODE_A])
                camera->translate(-camera->right * speed * dTime);
            if (k_state[SDL_SCANCODE_D])
                camera->translate(camera->right * speed * dTime);
            
            // Add vertical movement (Q/E for up/down)
            if (k_state[SDL_SCANCODE_Q])
                camera->translate(-camera->up * speed * dTime);
            if (k_state[SDL_SCANCODE_E])
                camera->translate(camera->up * speed * dTime);

            // Apply rotation and update instance buffer
            float rspeed = fast ? 180.0f : (slow ? 10.0f : 30.0f);
            if (k_state[SDL_SCANCODE_LEFT]) {
                models[0].rotateY(glm::radians(-rspeed * dTime));
                models[1].rotateY(glm::radians(-rspeed * dTime));
            }
            if (k_state[SDL_SCANCODE_RIGHT]) {
                models[0].rotateY(glm::radians(rspeed * dTime));
                models[1].rotateY(glm::radians(rspeed * dTime));
            }

            // Update instance buffers for both models
            std::vector<InstanceData> instances0, instances1;
            
            InstanceData instanceData0{};
            instanceData0.modelMatrix = models[0].getModelMatrix();
            instances0.push_back(instanceData0);
            buffer->updateInstanceBufferForMesh(0, instances0);
            
            InstanceData instanceData1{};
            instanceData1.modelMatrix = models[1].getModelMatrix();
            instances1.push_back(instanceData1);
            buffer->updateInstanceBufferForMesh(1, instances1);

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
