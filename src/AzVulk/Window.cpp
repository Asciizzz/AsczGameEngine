#include "AzVulk/Window.h"

#include <stdexcept>
#include <vulkan/vulkan.h>

namespace AzVulk {

Window::Window(int w, int h, const char* title)
    : width(w), height(h), title(title)
{
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow(
        title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN
    );
}

Window::~Window() {
    SDL_DestroyWindow(window);
    SDL_Quit();
}


std::vector<const char*> Window::getRequiredVulkanExtensions() {
    unsigned int count;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr)) {
        throw std::runtime_error("Failed to get Vulkan instance extensions: " + std::string(SDL_GetError()));
    }

    std::vector<const char*> extensions(count);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data())) {
        throw std::runtime_error("Failed to get Vulkan instance extensions: " + std::string(SDL_GetError()));
    }

    return extensions;
}

void Window::createVkSurface(VkInstance instance) {
    if (!SDL_Vulkan_CreateSurface(window, instance, &surface))
        throw std::runtime_error("Failed to create Vulkan surface: " + std::string(SDL_GetError()));
}
void Window::destroyVkSurface(VkInstance instance) {
    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }
}

} // namespace AzVulk