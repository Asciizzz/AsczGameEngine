#include "AzVulk/Window.h"

#include <stdexcept>

namespace AzVulk {

Window::Window(int w, int h, const char* title)
    : width(w), height(h), title(title)
{
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow(
        title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN
    );

    if (!window) {
        throw std::runtime_error("Failed to create SDL window: " + std::string(SDL_GetError()));
    }
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

void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) {
    if (SDL_Vulkan_CreateSurface(window, instance, surface) != SDL_TRUE) {
        throw std::runtime_error("Failed to create Vulkan surface: " + std::string(SDL_GetError()));
    }
}

VkExtent2D Window::getExtent() {
    return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

} // namespace AzVulk