#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <vector>

namespace AzVulk {

class Window {
public:
    int width, height;
    const char* title;

    Window(int w=800, int h=600, const char* title="69 Window");
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    std::vector<const char*> getRequiredVulkanExtensions();
    void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
    VkExtent2D getExtent();

    SDL_Window* window = nullptr;
};

} // namespace AzVulk