#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vector>

namespace AzVulk {

class Window {
public:
    int width, height;
    const char* title;

    Window(int w=800, int h=600, const char* title="69 Window");
    ~Window();

    std::vector<const char*> getRequiredVulkanExtensions();

    void createVkSurface(VkInstance instance);
    void destroyVkSurface(VkInstance instance);

private:
    SDL_Window* window;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
};

} // namespace AzVulk