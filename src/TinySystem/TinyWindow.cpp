#include "TinySystem/TinyWindow.hpp"
#include <iostream>
#include <stdexcept>

TinyWindow::TinyWindow(const char* title, uint32_t width, uint32_t height)
    : window(nullptr), shouldCloseFlag(false), resizedFlag(false), 
        windowWidth(width), windowHeight(height) {
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        throw std::runtime_error("Failed to initialize SDL");
    }

    window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        width,
        height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        SDL_Quit();
        throw std::runtime_error("Failed to create SDL window");
    }
}

TinyWindow::~TinyWindow() {
    if (window) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
}

void TinyWindow::pollEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                shouldCloseFlag = true;
                break;
            case SDL_WINDOWEVENT:
                // Only handle events for THIS window
                if (SDL_GetWindowFromID(e.window.windowID) == window) {
                    if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                        resizedFlag = true;
                    } else if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
                        shouldCloseFlag = true;
                    }
                }
                break;
        }
    }
}

std::vector<const char*> TinyWindow::getRequiredVulkanExtensions() const {
    unsigned int sdlExtensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, nullptr)) {
        throw std::runtime_error("Failed to get Vulkan extension count from SDL.");
    }

    std::vector<const char*> extensions(sdlExtensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, extensions.data())) {
        throw std::runtime_error("Failed to get Vulkan extensions from SDL.");
    }

    return extensions;
}

void TinyWindow::getFrameBufferSize(int& width, int& height) const {
    SDL_Vulkan_GetDrawableSize(window, &width, &height);
}

void TinyWindow::waitEvents() const {
    SDL_WaitEvent(nullptr);
}
