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

        initFpsOverlay();
        initVulkan();
    }

    Application::~Application() {
        cleanupFpsOverlay();
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

    // Playground - Load Az3D meshes from OBJ files!
        
        auto vikingRoomMesh = Az3D::Mesh::loadFromOBJ("Model/viking_room.obj");
        meshes.push_back(vikingRoomMesh);

        // Load mesh to buffer
        for (const auto& mesh : meshes)
            buffer->loadMeshToBuffer(*mesh);

        // Create single model at origin
        models.resize(1);
        models[0].setMesh(meshes[0]);
        models[0].position = glm::vec3(0.0f, 0.0f, 0.0f);
        models[0].rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

        // Create instance buffer for single model
        std::vector<InstanceData> instances;
        InstanceData instanceData{};
        instanceData.modelMatrix = models[0].getModelMatrix();
        instances.push_back(instanceData);

        buffer->createInstanceBufferForMesh(0, instances);

        textureManager = std::make_unique<TextureManager>(*vulkanDevice, commandPool);
        textureManager->createTextureImage("Model/viking_room.png");
        textureManager->createTextureImageView();
        textureManager->createTextureSampler();
        
        // Create descriptor manager with texture support
        descriptorManager = std::make_unique<DescriptorManager>(*vulkanDevice, graphicsPipelines[pipelineIndex]->descriptorSetLayout);
        descriptorManager->createDescriptorPool(2); // MAX_FRAMES_IN_FLIGHT
        descriptorManager->createDescriptorSets(buffer->uniformBuffers, sizeof(UniformBufferObject),
                                                textureManager->textureImageView, textureManager->textureSampler);

        
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
            models[0].rotateY(glm::radians(30.0f * dTime));

            // Update instance buffer with new rotation
            std::vector<InstanceData> instances;
            InstanceData instanceData{};
            instanceData.modelMatrix = models[0].getModelMatrix();
            instances.push_back(instanceData);
            buffer->updateInstanceBufferForMesh(0, instances);

            renderer->drawFrameWithModels(models, *graphicsPipelines[pipelineIndex]);
            renderFpsOverlay();
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

    void Application::initFpsOverlay() {
        fpsWindow = SDL_CreateWindow("FPS Monitor", 
                                    50, 50, 240, 80,
                                    SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_BORDERLESS | 
                                    SDL_WINDOW_SKIP_TASKBAR | SDL_WINDOW_UTILITY);
        
        if (!fpsWindow) {
            std::cerr << "Warning: Could not create FPS overlay window: " << SDL_GetError() << std::endl;
            return;
        }

        SDL_SetWindowOpacity(fpsWindow, 0.9f);
        SDL_RaiseWindow(fpsWindow);
        SDL_RaiseWindow(windowManager->window);

        fpsRenderer = SDL_CreateRenderer(fpsWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
        if (!fpsRenderer) {
            std::cerr << "Warning: Could not create FPS overlay renderer: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(fpsWindow);
            fpsWindow = nullptr;
            return;
        }

        fpsTexture = SDL_CreateTexture(fpsRenderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 240, 80);
        if (!fpsTexture) {
            std::cerr << "Warning: Could not create FPS texture: " << SDL_GetError() << std::endl;
            SDL_DestroyRenderer(fpsRenderer);
            SDL_DestroyWindow(fpsWindow);
            fpsRenderer = nullptr;
            fpsWindow = nullptr;
            return;
        }

        SDL_SetTextureBlendMode(fpsTexture, SDL_BLENDMODE_BLEND);

        lastFpsUpdate = std::chrono::steady_clock::now();
    }

    void Application::renderFpsOverlay() {
        if (!fpsRenderer || !fpsTexture) return;

        auto now = std::chrono::steady_clock::now();
        auto timeSinceUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsUpdate);

        if (timeSinceUpdate.count() < 1000) return;

        lastFpsUpdate = now;

        static int raiseCounter = 0;
        if (++raiseCounter % 5 == 0) {
            SDL_RaiseWindow(fpsWindow);
        }

        // Set render target to our FPS texture
        SDL_SetRenderTarget(fpsRenderer, fpsTexture);
        
        SDL_SetRenderDrawColor(fpsRenderer, 0, 0, 0, 255);
        SDL_RenderClear(fpsRenderer);

        // Get FPS values
        int currentFPS = static_cast<int>(fpsManager->currentFPS);
        int avgFPS = static_cast<int>(fpsManager->getAverageFPS());
        float frameTimeMs = fpsManager->frameTimeMs;
        int frameTimeTenths = static_cast<int>(frameTimeMs * 10);

        // Draw simple bar graphs for FPS
        SDL_SetRenderDrawColor(fpsRenderer, 255, 255, 0, 255);

        // Current FPS bar
        int barWidth1 = (currentFPS * 180) / 1000;
        if (barWidth1 > 180) barWidth1 = 180;
        SDL_Rect fpsBar1 = {10, 8, barWidth1, 12};
        SDL_RenderFillRect(fpsRenderer, &fpsBar1);

        // Average FPS bar
        SDL_SetRenderDrawColor(fpsRenderer, 0, 255, 255, 255); // Cyan
        int barWidth2 = (avgFPS * 180) / 1000;
        if (barWidth2 > 180) barWidth2 = 180;
        SDL_Rect fpsBar2 = {10, 28, barWidth2, 12};
        SDL_RenderFillRect(fpsRenderer, &fpsBar2);

        // Frame time bar
        SDL_SetRenderDrawColor(fpsRenderer, 255, 128, 0, 255); // Orange
        int barWidth3 = (frameTimeTenths * 180) / 200;
        if (barWidth3 > 180) barWidth3 = 180;
        SDL_Rect frameTimeBar = {10, 48, barWidth3, 12};
        SDL_RenderFillRect(fpsRenderer, &frameTimeBar);

        drawSimpleNumber(195, 8, currentFPS);
        drawSimpleNumber(195, 28, avgFPS);
        drawDecimalNumber(195, 48, frameTimeTenths); // Display frame time in tenths of milliseconds with decimal

        SDL_SetRenderTarget(fpsRenderer, nullptr);

        SDL_SetRenderDrawColor(fpsRenderer, 0, 0, 0, 255);
        SDL_RenderClear(fpsRenderer);

        SDL_Rect dstRect = {0, 0, 240, 80};
        SDL_RenderCopy(fpsRenderer, fpsTexture, nullptr, &dstRect);

        SDL_RenderPresent(fpsRenderer);
    }

    void Application::drawSimpleNumber(int x, int y, int number) {
        // Simple 7-segment style display using rectangles - thicker for better readability
        std::string numStr = std::to_string(number);

        SDL_SetRenderDrawColor(fpsRenderer, 255, 255, 255, 255); // White

        for (size_t i = 0; i < numStr.length() && i < 4; ++i) {
            char digit = numStr[i];
            int digitX = x + (i * 10); // More spacing between digits
            
            // Draw simple digit representation using thicker rectangles/lines
            switch (digit) {
                case '0': {
                    SDL_Rect rect = {digitX, y, 8, 12}; // Bigger digits
                    SDL_RenderDrawRect(fpsRenderer, &rect);
                    // Add thickness with another rectangle inside
                    SDL_Rect rect2 = {digitX + 1, y + 1, 6, 10};
                    SDL_RenderDrawRect(fpsRenderer, &rect2);
                    break;
                }
                case '1': {
                    // Draw thicker vertical line
                    SDL_Rect rect = {digitX + 3, y, 2, 12};
                    SDL_RenderFillRect(fpsRenderer, &rect);
                    break;
                }
                case '2': {
                    // Horizontal lines - thicker
                    SDL_Rect top = {digitX, y, 8, 2};
                    SDL_Rect mid = {digitX, y + 5, 8, 2};
                    SDL_Rect bot = {digitX, y + 10, 8, 2};
                    SDL_RenderFillRect(fpsRenderer, &top);
                    SDL_RenderFillRect(fpsRenderer, &mid);
                    SDL_RenderFillRect(fpsRenderer, &bot);
                    // Right vertical segment
                    SDL_Rect right1 = {digitX + 6, y, 2, 5};
                    SDL_RenderFillRect(fpsRenderer, &right1);
                    // Left vertical segment
                    SDL_Rect left2 = {digitX, y + 7, 2, 5};
                    SDL_RenderFillRect(fpsRenderer, &left2);
                    break;
                }
                case '3': {
                    // Horizontal lines
                    SDL_Rect top = {digitX, y, 8, 2};
                    SDL_Rect mid = {digitX, y + 5, 8, 2};
                    SDL_Rect bot = {digitX, y + 10, 8, 2};
                    SDL_RenderFillRect(fpsRenderer, &top);
                    SDL_RenderFillRect(fpsRenderer, &mid);
                    SDL_RenderFillRect(fpsRenderer, &bot);
                    // Right vertical line
                    SDL_Rect right = {digitX + 6, y, 2, 12};
                    SDL_RenderFillRect(fpsRenderer, &right);
                    break;
                }
                case '4': {
                    // Left vertical top half
                    SDL_Rect left = {digitX, y, 2, 6};
                    SDL_RenderFillRect(fpsRenderer, &left);
                    // Middle horizontal
                    SDL_Rect mid = {digitX, y + 5, 8, 2};
                    SDL_RenderFillRect(fpsRenderer, &mid);
                    // Right vertical full
                    SDL_Rect right = {digitX + 6, y, 2, 12};
                    SDL_RenderFillRect(fpsRenderer, &right);
                    break;
                }
                case '5': {
                    // Horizontal lines
                    SDL_Rect top = {digitX, y, 8, 2};
                    SDL_Rect mid = {digitX, y + 5, 8, 2};
                    SDL_Rect bot = {digitX, y + 10, 8, 2};
                    SDL_RenderFillRect(fpsRenderer, &top);
                    SDL_RenderFillRect(fpsRenderer, &mid);
                    SDL_RenderFillRect(fpsRenderer, &bot);
                    // Left vertical top half
                    SDL_Rect left1 = {digitX, y, 2, 6};
                    SDL_RenderFillRect(fpsRenderer, &left1);
                    // Right vertical bottom half
                    SDL_Rect right2 = {digitX + 6, y + 6, 2, 6};
                    SDL_RenderFillRect(fpsRenderer, &right2);
                    break;
                }
                case '6': {
                    SDL_Rect rect = {digitX, y, 8, 12};
                    SDL_RenderDrawRect(fpsRenderer, &rect);
                    SDL_Rect rect2 = {digitX + 1, y + 1, 6, 10};
                    SDL_RenderDrawRect(fpsRenderer, &rect2);
                    // Middle horizontal
                    SDL_Rect mid = {digitX, y + 5, 8, 2};
                    SDL_RenderFillRect(fpsRenderer, &mid);
                    break;
                }
                case '7': {
                    // Top horizontal
                    SDL_Rect top = {digitX, y, 8, 2};
                    SDL_RenderFillRect(fpsRenderer, &top);
                    // Right vertical
                    SDL_Rect right = {digitX + 6, y, 2, 12};
                    SDL_RenderFillRect(fpsRenderer, &right);
                    break;
                }
                case '8': {
                    SDL_Rect rect = {digitX, y, 8, 12};
                    SDL_RenderDrawRect(fpsRenderer, &rect);
                    SDL_Rect rect2 = {digitX + 1, y + 1, 6, 10};
                    SDL_RenderDrawRect(fpsRenderer, &rect2);
                    // Middle horizontal
                    SDL_Rect mid = {digitX, y + 5, 8, 2};
                    SDL_RenderFillRect(fpsRenderer, &mid);
                    break;
                }
                case '9': {
                    SDL_Rect rect = {digitX, y, 8, 7};
                    SDL_RenderDrawRect(fpsRenderer, &rect);
                    SDL_Rect rect2 = {digitX + 1, y + 1, 6, 5};
                    SDL_RenderDrawRect(fpsRenderer, &rect2);
                    // Right vertical full
                    SDL_Rect right = {digitX + 6, y, 2, 12};
                    SDL_RenderFillRect(fpsRenderer, &right);
                    // Bottom horizontal
                    SDL_Rect bot = {digitX, y + 10, 8, 2};
                    SDL_RenderFillRect(fpsRenderer, &bot);
                    break;
                }
            }
        }
    }

    void Application::drawDecimalNumber(int x, int y, int tenths) {
        // Draw decimal number like "1.3" from input like 13 (representing 1.3)
        // Extract integer and decimal parts
        int integerPart = tenths / 10;
        int decimalPart = tenths % 10;
        
        SDL_SetRenderDrawColor(fpsRenderer, 255, 255, 255, 255); // White
        
        int currentX = x;
        
        // Draw integer part (can be multiple digits)
        std::string intStr = std::to_string(integerPart);
        for (size_t i = 0; i < intStr.length() && i < 3; ++i) { // Max 3 digits for integer part
            char digit = intStr[i];
            drawSingleDigit(currentX, y, digit);
            currentX += 10; // Move to next position
        }
        
        // Draw decimal point
        SDL_Rect dot = {currentX + 2, y + 9, 2, 2}; // Small dot near bottom
        SDL_RenderFillRect(fpsRenderer, &dot);
        currentX += 6; // Move past the dot
        
        // Draw decimal part (single digit)
        char decimalDigit = '0' + decimalPart;
        drawSingleDigit(currentX, y, decimalDigit);
    }

    void Application::drawSingleDigit(int x, int y, char digit) {
        // Helper function to draw a single digit - same logic as in drawSimpleNumber
        int digitX = x;
        
        // Draw simple digit representation using thicker rectangles/lines
        switch (digit) {
            case '0': {
                SDL_Rect rect = {digitX, y, 8, 12}; // Bigger digits
                SDL_RenderDrawRect(fpsRenderer, &rect);
                // Add thickness with another rectangle inside
                SDL_Rect rect2 = {digitX + 1, y + 1, 6, 10};
                SDL_RenderDrawRect(fpsRenderer, &rect2);
                break;
            }
            case '1': {
                // Draw thicker vertical line
                SDL_Rect rect = {digitX + 3, y, 2, 12};
                SDL_RenderFillRect(fpsRenderer, &rect);
                break;
            }
            case '2': {
                // Horizontal lines - thicker
                SDL_Rect top = {digitX, y, 8, 2};
                SDL_Rect mid = {digitX, y + 5, 8, 2};
                SDL_Rect bot = {digitX, y + 10, 8, 2};
                SDL_RenderFillRect(fpsRenderer, &top);
                SDL_RenderFillRect(fpsRenderer, &mid);
                SDL_RenderFillRect(fpsRenderer, &bot);
                // Right vertical segment
                SDL_Rect right1 = {digitX + 6, y, 2, 5};
                SDL_RenderFillRect(fpsRenderer, &right1);
                // Left vertical segment
                SDL_Rect left2 = {digitX, y + 7, 2, 5};
                SDL_RenderFillRect(fpsRenderer, &left2);
                break;
            }
            case '3': {
                // Horizontal lines
                SDL_Rect top = {digitX, y, 8, 2};
                SDL_Rect mid = {digitX, y + 5, 8, 2};
                SDL_Rect bot = {digitX, y + 10, 8, 2};
                SDL_RenderFillRect(fpsRenderer, &top);
                SDL_RenderFillRect(fpsRenderer, &mid);
                SDL_RenderFillRect(fpsRenderer, &bot);
                // Right vertical line
                SDL_Rect right = {digitX + 6, y, 2, 12};
                SDL_RenderFillRect(fpsRenderer, &right);
                break;
            }
            case '4': {
                // Left vertical top half
                SDL_Rect left = {digitX, y, 2, 6};
                SDL_RenderFillRect(fpsRenderer, &left);
                // Middle horizontal
                SDL_Rect mid = {digitX, y + 5, 8, 2};
                SDL_RenderFillRect(fpsRenderer, &mid);
                // Right vertical full
                SDL_Rect right = {digitX + 6, y, 2, 12};
                SDL_RenderFillRect(fpsRenderer, &right);
                break;
            }
            case '5': {
                // Horizontal lines
                SDL_Rect top = {digitX, y, 8, 2};
                SDL_Rect mid = {digitX, y + 5, 8, 2};
                SDL_Rect bot = {digitX, y + 10, 8, 2};
                SDL_RenderFillRect(fpsRenderer, &top);
                SDL_RenderFillRect(fpsRenderer, &mid);
                SDL_RenderFillRect(fpsRenderer, &bot);
                // Left vertical top half
                SDL_Rect left1 = {digitX, y, 2, 6};
                SDL_RenderFillRect(fpsRenderer, &left1);
                // Right vertical bottom half
                SDL_Rect right2 = {digitX + 6, y + 6, 2, 6};
                SDL_RenderFillRect(fpsRenderer, &right2);
                break;
            }
            case '6': {
                SDL_Rect rect = {digitX, y, 8, 12};
                SDL_RenderDrawRect(fpsRenderer, &rect);
                SDL_Rect rect2 = {digitX + 1, y + 1, 6, 10};
                SDL_RenderDrawRect(fpsRenderer, &rect2);
                // Middle horizontal
                SDL_Rect mid = {digitX, y + 5, 8, 2};
                SDL_RenderFillRect(fpsRenderer, &mid);
                break;
            }
            case '7': {
                // Top horizontal
                SDL_Rect top = {digitX, y, 8, 2};
                SDL_RenderFillRect(fpsRenderer, &top);
                // Right vertical
                SDL_Rect right = {digitX + 6, y, 2, 12};
                SDL_RenderFillRect(fpsRenderer, &right);
                break;
            }
            case '8': {
                SDL_Rect rect = {digitX, y, 8, 12};
                SDL_RenderDrawRect(fpsRenderer, &rect);
                SDL_Rect rect2 = {digitX + 1, y + 1, 6, 10};
                SDL_RenderDrawRect(fpsRenderer, &rect2);
                // Middle horizontal
                SDL_Rect mid = {digitX, y + 5, 8, 2};
                SDL_RenderFillRect(fpsRenderer, &mid);
                break;
            }
            case '9': {
                SDL_Rect rect = {digitX, y, 8, 7};
                SDL_RenderDrawRect(fpsRenderer, &rect);
                SDL_Rect rect2 = {digitX + 1, y + 1, 6, 5};
                SDL_RenderDrawRect(fpsRenderer, &rect2);
                // Right vertical full
                SDL_Rect right = {digitX + 6, y, 2, 12};
                SDL_RenderFillRect(fpsRenderer, &right);
                // Bottom horizontal
                SDL_Rect bot = {digitX, y + 10, 8, 2};
                SDL_RenderFillRect(fpsRenderer, &bot);
                break;
            }
        }
    }

    void Application::cleanupFpsOverlay() {
        if (fpsTexture) {
            SDL_DestroyTexture(fpsTexture);
            fpsTexture = nullptr;
        }
        
        if (fpsRenderer) {
            SDL_DestroyRenderer(fpsRenderer);
            fpsRenderer = nullptr;
        }
        
        if (fpsWindow) {
            SDL_DestroyWindow(fpsWindow);
            fpsWindow = nullptr;
        }
    }
}
