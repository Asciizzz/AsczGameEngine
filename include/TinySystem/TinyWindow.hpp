#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vector>
#include <cstdint>

class TinyWindow {
public:
    TinyWindow(const char* title, uint32_t width, uint32_t height);
    ~TinyWindow();

    TinyWindow(const TinyWindow&) = delete;
    TinyWindow& operator=(const TinyWindow&) = delete;

    operator SDL_Window*() const { return window; }

    void pollEvents();
    std::vector<const char*> getRequiredVulkanExtensions() const;
    void getFrameBufferSize(int& width, int& height) const;
    void waitEvents() const;

    Uint32 toggleFullscreen();

    SDL_Window* window;
    bool shouldCloseFlag;
    bool resizedFlag;
    uint32_t windowWidth, windowHeight;
};