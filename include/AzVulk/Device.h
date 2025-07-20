#pragma once

#include <AzVulk/Helper.hpp>

namespace AzVulk {

class Device {
public:
#ifdef NDEBUG
    static constexpr bool enableValidationLayers = false;
#else
    static constexpr bool enableValidationLayers = true;
#endif
    int width, height;

    Device(int w, int h);
    ~Device(); void cleanup();

    // Not copyable or movable
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    void drawFrame(VkPipeline graphicsPipeline);

    // Getters
    VkDevice getDevice() const { return device; }
    SDL_Window* getWindow() const { return window; }
    VkInstance getInstance() const { return instance; }
    VkSurfaceKHR getSurface() const { return surface; }
    VkQueue getPresentQueue() const { return presentQueue; }
    VkQueue getGraphicsQueue() const { return graphicsQueue; }
    VkCommandPool getCommandPool() const { return commandPool; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }

private:
    SDL_Window* window;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;

    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkCommandPool commandPool;

    void init();

    void createWindow();
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();
};

} // namespace AzVulk