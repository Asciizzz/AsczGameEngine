#pragma once

#include <AzVulkHelper.hpp>

class AzVulk {
public:
#ifdef NDEBUG
    static constexpr bool enableValidationLayers = false;
#else
    static constexpr bool enableValidationLayers = true;
#endif

    static const std::vector<const char*> validationLayers;

    int width, height;

    AzVulk(int w, int h);
    ~AzVulk();

private: // No particular group, just split for clarity
    SDL_Window* window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE; // Logical device
    VkQueue graphicsQueue;
    VkQueue presentQueue;


    void init();
    void cleanup();

    void createWindow();
    void createInstance();
    void setupDebugMessenger();
    void pickPhysicalDevice();
    void createLogicalDevice();

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    bool IsDeviceSuitable(VkPhysicalDevice device);
};