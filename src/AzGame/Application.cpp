#include "AzGame/Application.hpp"
#include <SDL2/SDL_vulkan.h>
#include <stdexcept>

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
            {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
            {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
        };
        
        const std::vector<uint16_t> indices = {
            0, 1, 2, 2, 3, 0
        };
        
        buffer->createVertexBuffer(vertices);
        buffer->createIndexBuffer(indices);
        buffer->createUniformBuffers(2); // MAX_FRAMES_IN_FLIGHT
        
        // Create descriptor manager and sets
        descriptorManager = std::make_unique<DescriptorManager>(*vulkanDevice, graphicsPipeline->getDescriptorSetLayout());
        descriptorManager->createDescriptorPool(2); // MAX_FRAMES_IN_FLIGHT
        descriptorManager->createDescriptorSets(buffer->getUniformBuffers(), sizeof(UniformBufferObject));
        
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
