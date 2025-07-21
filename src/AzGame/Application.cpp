#include "AzGame/Application.hpp"
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
        
        swapChain->createFramebuffers(graphicsPipeline->getRenderPass());
        
        shaderManager = std::make_unique<ShaderManager>(vulkanDevice->getLogicalDevice());
        
        buffer = std::make_unique<Buffer>(*vulkanDevice);
        
        // Initialize buffers with test data
        const std::vector<Vertex> vertices = {
            {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
            {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
        };
        
        const std::vector<uint16_t> indices = {
            0, 1, 2, 2, 3, 0
        };
        
        buffer->createVertexBuffer(vertices);
        buffer->createIndexBuffer(indices);
        buffer->createUniformBuffers(2); // MAX_FRAMES_IN_FLIGHT
        
        // Create texture manager first - we'll use a temporary command pool approach
        // Initialize texture manager using VulkanDevice's temporary command pool approach
        // Note: You need to place a texture file called "texture.jpg" in the textures/ directory
        // You can use any image file (JPG, PNG, BMP, etc.) and rename it to texture.jpg
        try {
            // Create a temporary command pool for texture loading
            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolInfo.queueFamilyIndex = vulkanDevice->getQueueFamilyIndices().graphicsFamily.value();
            
            VkCommandPool tempCommandPool;
            if (vkCreateCommandPool(vulkanDevice->getLogicalDevice(), &poolInfo, nullptr, &tempCommandPool) != VK_SUCCESS) {
                throw std::runtime_error("failed to create temporary command pool!");
            }
            
            textureManager = std::make_unique<TextureManager>(*vulkanDevice, tempCommandPool);
            textureManager->createTextureImage("textures/texture.jpg");
            textureManager->createTextureImageView();
            textureManager->createTextureSampler();
            
            // Clean up temporary command pool
            vkDestroyCommandPool(vulkanDevice->getLogicalDevice(), tempCommandPool, nullptr);
            
        } catch (const std::runtime_error& e) {
            // If texture loading fails, create a procedural checkerboard texture instead
            std::cerr << "Warning: Could not load texture file - " << e.what() << std::endl;
            std::cerr << "Please place a texture image named 'texture.jpg' in the textures/ directory" << std::endl;
            throw; // Re-throw for now, but could create fallback texture
        }
        
        // Now create descriptor manager with texture support
        descriptorManager = std::make_unique<DescriptorManager>(*vulkanDevice, graphicsPipeline->getDescriptorSetLayout());
        descriptorManager->createDescriptorPool(2); // MAX_FRAMES_IN_FLIGHT
        descriptorManager->createDescriptorSets(buffer->getUniformBuffers(), sizeof(UniformBufferObject),
                                               textureManager->getTextureImageView(), textureManager->getTextureSampler());
        
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
                
                // Recreate swap chain
                swapChain->recreate(windowManager->getWindow());
                
                // Recreate graphics pipeline with new extent and format
                graphicsPipeline->recreate(swapChain->getExtent(), swapChain->getImageFormat());
                
                // Recreate framebuffers
                swapChain->createFramebuffers(graphicsPipeline->getRenderPass());
            }
            
            renderer->drawFrame();
        }

        vkDeviceWaitIdle(vulkanDevice->getLogicalDevice());
    }

    void Application::cleanup() {
        if (surface != VK_NULL_HANDLE && vulkanInstance) {
            vkDestroySurfaceKHR(vulkanInstance->getInstance(), surface, nullptr);
        }
    }
}
