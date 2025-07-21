#include "AzGame/Application.hpp"
#include "AzModel/AzModel.hpp"
#include <SDL2/SDL_vulkan.h>
#include <stdexcept>
#include <iostream>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

namespace AzGame {
    Application::Application(const char* title, uint32_t width, uint32_t height)
        : appTitle(title), appWidth(width), appHeight(height) {
        
        windowManager = std::make_unique<WindowManager>(title, width, height);
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
        
        vulkanDevice = std::make_unique<VulkanDevice>(vulkanInstance->getInstance(), surface);
        
        swapChain = std::make_unique<SwapChain>(*vulkanDevice, surface, windowManager->getWindow());
        
        graphicsPipeline = std::make_unique<GraphicsPipeline>(
            vulkanDevice->getLogicalDevice(), 
            swapChain->getExtent(), 
            swapChain->getImageFormat()
        );
        
        shaderManager = std::make_unique<ShaderManager>(vulkanDevice->getLogicalDevice());
        
        // Create command pool for model operations
        createModelCommandPool();


    // ==================================== PLAYGROUND  ====================================


        // Create two separate quad models with different textures for depth testing
        
        // Create front quad with texture1.png (Red/Blue checkerboard)
        auto frontQuad = AzModel::Model3D::createQuad(*vulkanDevice, modelCommandPool, 0.6f, "textures/texture1.png");
        frontQuad->setPosition({0.0f, 0.0f, 0.0f});  // Front position
        frontQuad->setName("FrontQuad");
        models.push_back(frontQuad);

        // Create back quad with texture2.png (Green/Yellow checkerboard) 
        auto backQuad = AzModel::Model3D::createQuad(*vulkanDevice, modelCommandPool, 1.4f, "textures/texture2.png");
        backQuad->setPosition({0.0f, 0.0f, -0.5f});  // Back position (behind the front quad)
        backQuad->setName("BackQuad");
        models.push_back(backQuad);

        std::cout << "Created " << models.size() << " separate quad models for depth testing:" << std::endl;
        for (const auto& model : models) {
            std::cout << "  - " << model->getName() << " at position (" 
                      << model->getPosition().x << ", " 
                      << model->getPosition().y << ", " 
                      << model->getPosition().z << ")" << std::endl;
        }
        
        // Set up buffers using the models for compatibility with existing system
        // TODO: Extend renderer to handle multiple models with different materials
        if (!models.empty()) {
            buffer = std::make_unique<Buffer>(*vulkanDevice);
            
            // For now, manually create geometry that matches our two quad models
            // This is a temporary bridge until renderer supports multiple models directly
            const std::vector<Vertex> vertices = {
                // Front quad (z = 0.0) - texture1.png, smaller size (0.6f)
                {{-0.3f, -0.3f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                {{ 0.3f, -0.3f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                {{ 0.3f,  0.3f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                {{-0.3f,  0.3f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

                // Back quad (z = -0.5) - texture2.png, larger size (1.4f)
                {{-0.7f, -0.7f, -0.5f}, {1.0f, 0.5f, 0.0f}, {0.0f, 0.0f}},
                {{ 0.7f, -0.7f, -0.5f}, {0.5f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                {{ 0.7f,  0.7f, -0.5f}, {0.0f, 0.5f, 1.0f}, {1.0f, 1.0f}},
                {{-0.7f,  0.7f, -0.5f}, {0.8f, 0.8f, 0.8f}, {0.0f, 1.0f}}
            };
            
            const std::vector<uint16_t> indices = {
                // Front quad indices
                0, 1, 2, 2, 3, 0,
                // Back quad indices
                4, 5, 6, 6, 7, 4
            };
            
            buffer->createVertexBuffer(vertices);
            buffer->createIndexBuffer(indices);
            buffer->createUniformBuffers(2); // MAX_FRAMES_IN_FLIGHT
        }
        
        // Use texture from the first model's material instead of hardcoded texture
        if (!models.empty() && !models[0]->getMeshes().empty()) {
            auto firstMesh = models[0]->getMeshes()[0];
            auto materials = firstMesh->getMaterials();
            if (!materials.empty()) {
                auto firstMaterial = materials.begin()->second;
                
                // Create a simple texture manager wrapper for compatibility
                // TODO: Remove TextureManager and use Material system directly
                textureManager = std::make_unique<TextureManager>(*vulkanDevice, modelCommandPool);
                // For now, we'll create a checkerboard texture in TextureManager
                // This maintains compatibility while transitioning to the new system
                try {
                    textureManager->createTextureImage("textures/texture1.png"); // Try existing texture
                    textureManager->createTextureImageView();
                    textureManager->createTextureSampler();
                } catch (const std::runtime_error& e) {
                    std::cerr << "Info: Using model materials instead of separate texture file" << std::endl;
                    // Create a fallback texture for compatibility
                    textureManager->createTextureImage(""); // This should create a fallback
                    textureManager->createTextureImageView();
                    textureManager->createTextureSampler();
                }
            }
        }
        
        // Create depth manager and depth resources
        depthManager = std::make_unique<DepthManager>(*vulkanDevice);
        depthManager->createDepthResources(swapChain->getExtent().width, swapChain->getExtent().height);
        
        // Now create descriptor manager with texture support
        descriptorManager = std::make_unique<DescriptorManager>(*vulkanDevice, graphicsPipeline->getDescriptorSetLayout());
        descriptorManager->createDescriptorPool(2); // MAX_FRAMES_IN_FLIGHT
        descriptorManager->createDescriptorSets(buffer->getUniformBuffers(), sizeof(UniformBufferObject),
                                               textureManager->getTextureImageView(), textureManager->getTextureSampler());
        
        // Create framebuffers with depth buffer support
        swapChain->createFramebuffers(graphicsPipeline->getRenderPass(), depthManager->getDepthImageView());
        
        // Create final renderer with texture-enabled descriptor manager
        renderer = std::make_unique<Renderer>(*vulkanDevice, *swapChain, *graphicsPipeline, *buffer, *descriptorManager);
    }

    void Application::createSurface() {
        if (!SDL_Vulkan_CreateSurface(windowManager->getWindow(), vulkanInstance->getInstance(), &surface)) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void Application::mainLoop() {
        while (!windowManager->shouldClose()) {
            windowManager->pollEvents();
            
            if (windowManager->wasResized() || renderer->isFramebufferResized()) {
                windowManager->resetResizedFlag();
                renderer->setFramebufferResized(false);
                
                // Recreate depth resources for new window size
                depthManager->createDepthResources(swapChain->getExtent().width, swapChain->getExtent().height);
                
                // Recreate swap chain with depth support
                swapChain->recreate(windowManager->getWindow(), graphicsPipeline->getRenderPass(), depthManager->getDepthImageView());
                
                // Recreate graphics pipeline with new extent, format, and depth format
                graphicsPipeline->recreate(swapChain->getExtent(), swapChain->getImageFormat(), depthManager->getDepthFormat());
            }
            
            renderer->drawFrame();
        }

        vkDeviceWaitIdle(vulkanDevice->getLogicalDevice());
    }

    void Application::cleanup() {
        if (modelCommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(vulkanDevice->getLogicalDevice(), modelCommandPool, nullptr);
        }
        
        if (surface != VK_NULL_HANDLE && vulkanInstance) {
            vkDestroySurfaceKHR(vulkanInstance->getInstance(), surface, nullptr);
        }
    }

    void Application::createModelCommandPool() {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = vulkanDevice->getQueueFamilyIndices().graphicsFamily.value();
        
        if (vkCreateCommandPool(vulkanDevice->getLogicalDevice(), &poolInfo, nullptr, &modelCommandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create model command pool!");
        }
    }
}
