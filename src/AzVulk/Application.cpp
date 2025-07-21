#include "AzVulk/Application.hpp"
#include <SDL2/SDL_vulkan.h>
#include <stdexcept>
#include <iostream>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

namespace AzVulk {
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
            swapChain->getImageFormat(),
            "Shaders/hello.vert.spv",
            "Shaders/hello.frag.spv"
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
        
        // Create buffer with simple hardcoded geometry
        buffer = std::make_unique<Buffer>(*vulkanDevice);



        const std::vector<Vertex> vertices = {
            {{-0.3f, -0.3f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
            {{ 0.3f, -0.3f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
            {{ 0.3f,  0.3f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
            {{-0.3f,  0.3f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},

            {{-0.7f, -0.7f, -0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
            {{ 0.7f, -0.7f, -0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
            {{ 0.7f,  0.7f, -0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
            {{-0.7f,  0.7f, -0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}
        };

        const std::vector<uint16_t> indices = {
            0, 1, 2, 2, 3, 0,
            4, 5, 6, 6, 7, 4
        };

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
        
        // Create depth manager and depth resources
        depthManager = std::make_unique<DepthManager>(*vulkanDevice);
        depthManager->createDepthResources(swapChain->getExtent().width, swapChain->getExtent().height);
        
        // Create descriptor manager with texture support
        descriptorManager = std::make_unique<DescriptorManager>(*vulkanDevice, graphicsPipeline->getDescriptorSetLayout());
        descriptorManager->createDescriptorPool(2); // MAX_FRAMES_IN_FLIGHT
        descriptorManager->createDescriptorSets(buffer->getUniformBuffers(), sizeof(UniformBufferObject),
                                               textureManager->getTextureImageView(), textureManager->getTextureSampler());
        
        // Create framebuffers with depth buffer support
        swapChain->createFramebuffers(graphicsPipeline->getRenderPass(), depthManager->getDepthImageView());
        
        // Create final renderer
        renderer = std::make_unique<Renderer>(*vulkanDevice, *swapChain, *graphicsPipeline, *buffer, *descriptorManager);
        
        std::cout << "Application initialized with simple hardcoded geometry and depth testing." << std::endl;
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
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(vulkanDevice->getLogicalDevice(), commandPool, nullptr);
        }
        
        if (surface != VK_NULL_HANDLE && vulkanInstance) {
            vkDestroySurfaceKHR(vulkanInstance->getInstance(), surface, nullptr);
        }
    }
}
