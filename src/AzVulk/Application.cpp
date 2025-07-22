#include "AzVulk/Application.hpp"
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
        
        vulkanDevice = std::make_unique<VulkanDevice>(vulkanInstance->getInstance(), surface);
        
        // Create MSAA manager first to get sample count
        msaaManager = std::make_unique<MSAAManager>(*vulkanDevice);
        
        swapChain = std::make_unique<SwapChain>(*vulkanDevice, surface, windowManager->getWindow());
        
        graphicsPipeline = std::make_unique<GraphicsPipeline>(
            vulkanDevice->getLogicalDevice(), 
            swapChain->getExtent(), 
            swapChain->getImageFormat(),
            "Shaders/hello.vert.spv",
            "Shaders/hello.frag.spv",
            msaaManager->getMSAASamples()
        );
        
        shaderManager = std::make_unique<ShaderManager>(vulkanDevice->getLogicalDevice());
        
        // Create command pool for operations
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = vulkanDevice->getQueueFamilyIndices().graphicsFamily.value();
        
        if (vkCreateCommandPool(vulkanDevice->getLogicalDevice(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
        
        // Create buffer with procedurally generated grid geometry for benchmarking
        buffer = std::make_unique<Buffer>(*vulkanDevice);

        // Generate a grid of vertices expanding from origin (0,0,0)
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices; // Changed from uint16_t to uint32_t for large grids
        
        const int gridSize = 500; // 50x50 grid = 2500 vertices
        const float spacing = 0.005f; // Distance between grid points
        const float halfGrid = (gridSize - 1) * spacing * 0.5f; // Center the grid
        
        // Generate vertices in a grid pattern
        for (int z = 0; z < gridSize; z++) {
            for (int x = 0; x < gridSize; x++) {
                float worldX = (x * spacing) - halfGrid; // Range from -halfGrid to +halfGrid
                float worldZ = (z * spacing) - halfGrid; // Range from -halfGrid to +halfGrid
                float worldY = 0.0f; // Keep all points on the same plane for now
                
                // Calculate texture coordinates (0 to 1)
                float texU = static_cast<float>(x) / (gridSize - 1);
                float texV = static_cast<float>(z) / (gridSize - 1);

                vertices.push_back({
                    {worldX, worldY, worldZ},           // Position
                    {0.0f, 1.0f, 0.0f},                 // Normal (upward)
                    {texU, texV}                        // Texture coordinates
                });
            }
        }
        
        // Generate indices to create triangles (two triangles per grid square)
        for (int z = 0; z < gridSize - 1; z++) {
            for (int x = 0; x < gridSize - 1; x++) {
                // Calculate vertex indices for the current grid square
                uint32_t topLeft = z * gridSize + x;
                uint32_t topRight = z * gridSize + (x + 1);
                uint32_t bottomLeft = (z + 1) * gridSize + x;
                uint32_t bottomRight = (z + 1) * gridSize + (x + 1);

                // Add Counter-clockwise order to avoid backface culling

                // First triangle (top-left, bottom-left, top-right and reverse)
                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);
                
                // Second triangle (top-right, bottom-left, bottom-right and reverse)
                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
        }
        
        std::cout << "Generated grid with " << vertices.size() << " vertices and " 
                  << indices.size() / 3 << " triangles for MSAA benchmarking" << std::endl;

        buffer->createVertexBuffer(vertices);
        buffer->createIndexBuffer(indices);
        buffer->createUniformBuffers(2); // MAX_FRAMES_IN_FLIGHT
        
        // Create texture manager with simple texture loading
        textureManager = std::make_unique<TextureManager>(*vulkanDevice, commandPool);
        try {
            textureManager->createTextureImage("textures/texture1.png");
            textureManager->createTextureImageView();
            textureManager->createTextureSampler();
        } catch (const std::runtime_error& e) {
            std::cerr << "Warning: Could not load texture1.png, using fallback" << std::endl;
            // TextureManager should handle fallback automatically
            textureManager->createTextureImage("");
            textureManager->createTextureImageView();
            textureManager->createTextureSampler();
        }
        
        // Create MSAA color resources
        msaaManager->createColorResources(swapChain->getExtent().width, swapChain->getExtent().height, swapChain->getImageFormat());
        
        // Create depth manager and depth resources
        depthManager = std::make_unique<DepthManager>(*vulkanDevice);
        depthManager->createDepthResources(swapChain->getExtent().width, swapChain->getExtent().height, msaaManager->getMSAASamples());
        
        // Create descriptor manager with texture support
        descriptorManager = std::make_unique<DescriptorManager>(*vulkanDevice, graphicsPipeline->getDescriptorSetLayout());
        descriptorManager->createDescriptorPool(2); // MAX_FRAMES_IN_FLIGHT
        descriptorManager->createDescriptorSets(buffer->getUniformBuffers(), sizeof(UniformBufferObject),
                                                textureManager->getTextureImageView(), textureManager->getTextureSampler());
        
        // Create framebuffers with depth buffer and MSAA support
        swapChain->createFramebuffers(graphicsPipeline->getRenderPass(), depthManager->getDepthImageView(), msaaManager->getColorImageView());
        
        // Create final renderer
        renderer = std::make_unique<Renderer>(*vulkanDevice, *swapChain, *graphicsPipeline, *buffer, *descriptorManager);
        

        printf("Vertices count: %zu\n", vertices.size());
        printf("Faces count: %zu\n", indices.size() / 3);
    }

    void Application::createSurface() {
        if (!SDL_Vulkan_CreateSurface(windowManager->getWindow(), vulkanInstance->getInstance(), &surface)) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void Application::mainLoop() {

        bool q_hold = false;

        while (!windowManager->shouldClose()) {
            // Update FPS manager for timing
            fpsManager->update();
            windowManager->pollEvents();
            
            // Check if window was resized or renderer needs to be updated
            if (windowManager->wasResized() || renderer->isFramebufferResized()) {
                windowManager->resetResizedFlag();
                renderer->setFramebufferResized(false);
                
                // Wait for device to be idle before recreating resources
                vkDeviceWaitIdle(vulkanDevice->getLogicalDevice());
                
                // Get the new window size
                int newWidth, newHeight;
                SDL_GetWindowSize(windowManager->getWindow(), &newWidth, &newHeight);
                
                // Recreate MSAA color resources for new window size
                msaaManager->createColorResources(newWidth, newHeight, swapChain->getImageFormat());
                
                // Recreate depth resources for new window size FIRST
                depthManager->createDepthResources(newWidth, newHeight, msaaManager->getMSAASamples());
                
                // Recreate swap chain with depth and MSAA support
                swapChain->recreate(windowManager->getWindow(), graphicsPipeline->getRenderPass(), depthManager->getDepthImageView(), msaaManager->getColorImageView());
                
                // Recreate graphics pipeline with new extent, format, depth format, and MSAA samples
                graphicsPipeline->recreate(swapChain->getExtent(), swapChain->getImageFormat(), depthManager->getDepthFormat(), msaaManager->getMSAASamples());
            }

            const Uint8* k_state = SDL_GetKeyboardState(nullptr);
            if (k_state[SDL_SCANCODE_Q] && !q_hold) {
                q_hold = true;
                printf("Q pressed!");
            }
            if (!k_state[SDL_SCANCODE_Q]) {
                q_hold = false; // Reset hold state when key is released
            }

            renderer->drawFrame();
            renderFpsOverlay();
        }

        vkDeviceWaitIdle(vulkanDevice->getLogicalDevice());
    }

    void Application::cleanup() {
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(vulkanDevice->getLogicalDevice(), commandPool, nullptr);
        }
        
        if (surface != VK_NULL_HANDLE && vulkanInstance) {
            vkDestroySurfaceKHR(vulkanInstance->getInstance(), surface, nullptr);
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
        SDL_RaiseWindow(windowManager->getWindow());

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
        
        std::cout << "FPS overlay initialized (separate window mode)" << std::endl;
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
        int currentFPS = static_cast<int>(fpsManager->getFPS());
        int avgFPS = static_cast<int>(fpsManager->getAverageFPS());
        float frameTimeMs = fpsManager->getFrameTimeMs();
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
