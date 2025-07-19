#include "AzPlatform/Window.h"

namespace AzPlatform {

Window::Window(int w, int h, const char* title)
    : width(w), height(h), title(title)
{
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow(  title,
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED,
                                width, height,
                                SDL_WINDOW_VULKAN);
}

Window::~Window() {
    SDL_DestroyWindow(window);
    SDL_Quit();
}

} // namespace AzPlatform