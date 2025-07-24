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
    
        resourceManager = std::make_unique<Az3D::ResourceManager>(*vulkanDevice, commandPool);

// PLAYGROUND FROM HERE!

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
        
        // Use Az3D texture system with ResourceManager's string-based interface
        auto texture = resourceManager->getTexture("shiroko_diffuse");
        if (texture && texture->getData()) {
            descriptorManager->createDescriptorSets(buffer->uniformBuffers, sizeof(UniformBufferObject),
                                                    texture->getData()->view, texture->getData()->sampler);
        }

        // Final Renderer setup
        renderer = std::make_unique<Renderer>(*vulkanDevice, *swapChain, *graphicsPipelines[pipelineIndex], *buffer, *descriptorManager, *camera);
        // Print comprehensive resource summary
        printResourceSummary();
    }
    
    void Application::printResourceSummary() {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "             AZ3D RESOURCE INITIALIZATION COMPLETE" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        // Texture summary
        std::cout << "\n[TEXTURE RESOURCES]" << std::endl;
        std::cout << "  Total textures loaded: " << resourceManager->getTextureManager().getTextureCount() << std::endl;
        
        const auto& allTextures = resourceManager->getTextureManager().getAllTextures();
        for (size_t i = 0; i < allTextures.size(); ++i) {
            if (allTextures[i]) {
                const auto* tex = allTextures[i].get();
                std::cout << "  [" << i << "] " << tex->getId() 
                          << " (" << tex->getWidth() << "x" << tex->getHeight() 
                          << ", " << tex->getMipLevels() << " mips)";
                if (i == 0) std::cout << " [DEFAULT]";
                std::cout << std::endl;
                if (!tex->getFilePath().empty()) {
                    std::cout << "      Path: " << tex->getFilePath() << std::endl;
                }
            } else {
                std::cout << "  [" << i << "] <deleted>" << std::endl;
            }
        }
        
        // Material summary
        std::cout << "\n[MATERIAL RESOURCES]" << std::endl;
        auto materialIds = resourceManager->getMaterialManager().getMaterialIds();
        std::cout << "  Total materials loaded: " << materialIds.size() << std::endl;
        for (const auto& id : materialIds) {
            auto* mat = resourceManager->getMaterialManager().getMaterial(id);
            if (mat) {
                std::cout << "  * " << id << " (\"" << mat->getName() << "\")" << std::endl;
                std::cout << "    Roughness: " << mat->getProperties().roughness 
                          << ", Metallic: " << mat->getProperties().metallic << std::endl;
                if (!mat->getDiffuseTexture().empty()) {
                    std::cout << "    Diffuse: " << mat->getDiffuseTexture() 
                              << " -> Index " << resourceManager->getTextureIndex(mat->getDiffuseTexture()) << std::endl;
                }
            }
        }
        
        // Mesh summary
        std::cout << "\n[MESH RESOURCES]" << std::endl;
        auto meshIds = resourceManager->getMeshManager().getMeshIds();
        std::cout << "  Total meshes loaded: " << meshIds.size() << std::endl;
        for (const auto& id : meshIds) {
            auto* mesh = resourceManager->getMeshManager().getMesh(id);
            if (mesh) {
                std::cout << "  * " << id << " (" << mesh->getVertices().size() 
                          << " vertices, " << mesh->getIndices().size() << " indices)" << std::endl;
            }
        }
        
        // Model summary
        std::cout << "\n[SCENE MODELS]" << std::endl;
        std::cout << "  Total models: " << models.size() << std::endl;
        for (size_t i = 0; i < models.size(); ++i) {
            std::cout << "  [" << i << "] Mesh: " << models[i].getMeshId() 
                      << ", Material: " << models[i].getMaterialId() << std::endl;
            auto pos = models[i].position;
            std::cout << "      Position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
        }
        
        std::cout << "\n[READY] Use WASD + mouse to navigate, Tab to switch shaders, F1 to toggle mouse, F2 for FPS." << std::endl;
        std::cout << std::string(60, '=') << std::endl;
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
