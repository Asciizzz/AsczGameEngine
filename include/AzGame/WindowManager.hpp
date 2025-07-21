#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

namespace AzGame {
    class WindowManager {
    public:
        WindowManager(const char* title, uint32_t width, uint32_t height);
        ~WindowManager();

        // Delete copy constructor and assignment operator
        WindowManager(const WindowManager&) = delete;
        WindowManager& operator=(const WindowManager&) = delete;

        SDL_Window* getWindow() const { return window; }
        bool shouldClose() const { return shouldCloseFlag; }
        bool wasResized() const { return resizedFlag; }
        void resetResizedFlag() { resizedFlag = false; }
        
        void pollEvents();
        std::vector<const char*> getRequiredVulkanExtensions() const;
        void getFramebufferSize(int& width, int& height) const;
        void waitEvents() const;

    private:
        SDL_Window* window;
        bool shouldCloseFlag;
        bool resizedFlag;
        uint32_t windowWidth;
        uint32_t windowHeight;
    };
}
