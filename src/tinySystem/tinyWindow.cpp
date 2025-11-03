#include "tinySystem/tinyWindow.hpp"
#include <iostream>
#include <stdexcept>

#ifdef _WIN32
#include <SDL2/SDL_syswm.h>
#include <windows.h>
#endif

tinyWindow::tinyWindow(const char* title, uint32_t width, uint32_t height)
    : window(nullptr), shouldCloseFlag(false), resizedFlag(false), 
        width(width), height(height) {
    
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

    // Set window icon
    setWindowIcon("Ascz.ico");
}

tinyWindow::~tinyWindow() {
    if (window) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
}

void tinyWindow::pollEvents() {
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

std::vector<const char*> tinyWindow::getRequiredVulkanExtensions() const {
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

void tinyWindow::getFrameBufferSize(int& width, int& height) const {
    SDL_Vulkan_GetDrawableSize(window, &width, &height);
}

void tinyWindow::waitEvents() const {
    SDL_WaitEvent(nullptr);
}

Uint32 tinyWindow::toggleFullscreen() {
    Uint32 flags = SDL_GetWindowFlags(window);
    if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
        SDL_SetWindowFullscreen(window, 0);
    } else {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    return SDL_GetWindowFlags(window);
}

void tinyWindow::maximizeWindow() {
    SDL_MaximizeWindow(window);
}

void tinyWindow::setWindowIcon(const char* iconPath) {
#ifdef _WIN32
    // Use Windows API to load .ico file directly
    HICON hIcon = (HICON)LoadImageA(NULL, iconPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    
    if (!hIcon) {
        std::cerr << "Warning: Failed to load window icon from " << iconPath << std::endl;
        return;
    }

    // Get native Windows handle from SDL window
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    
    if (SDL_GetWindowWMInfo(window, &wmInfo)) {
        HWND hwnd = wmInfo.info.win.window;
        
        // Set both small and large icons
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    } else {
        std::cerr << "Warning: Failed to get Windows window info" << std::endl;
        DestroyIcon(hIcon);
    }
    
    // Note: Don't destroy icon here as Windows keeps a reference to it
#else
    std::cerr << "Warning: setWindowIcon is only supported on Windows" << std::endl;
#endif
}