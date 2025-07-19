#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

namespace AzPlatform {

class Window {
public:
    int width, height;
    const char* title;

    Window(int w=800, int h=600, const char* title="AzPlatform Window");
    ~Window();

private:
    SDL_Window* window;
};

} // namespace AzVulk