#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vector>
#include <cstdint>

class WindowManager {
public:
    WindowManager(const char* title, uint32_t width, uint32_t height);
    ~WindowManager();

    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;

    void pollEvents();
    std::vector<const char*> getRequiredVulkanExtensions() const;
    void getFramebufferSize(int& width, int& height) const;
    void waitEvents() const;
    
    SDL_Window* window;
    bool shouldCloseFlag;
    bool resizedFlag;
    uint32_t windowWidth, windowHeight;
};