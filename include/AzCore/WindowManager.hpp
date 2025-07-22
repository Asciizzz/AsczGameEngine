#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vector>
#include <cstdint>

namespace AzCore {
    class WindowManager {
    public:
        WindowManager(const char* title, uint32_t width, uint32_t height);
        ~WindowManager();

        // Delete copy constructor and assignment operator
        WindowManager(const WindowManager&) = delete;
        WindowManager& operator=(const WindowManager&) = delete;

        void pollEvents();
        
        bool shouldClose() const { return shouldCloseFlag; }
        bool wasResized() const { return resizedFlag; }
        void resetResizedFlag() { resizedFlag = false; }
        
        SDL_Window* getWindow() const { return window; }
        
        std::vector<const char*> getRequiredVulkanExtensions() const;
        void getFramebufferSize(int& width, int& height) const;
        void waitEvents() const;
        
        uint32_t getWidth() const { return windowWidth; }
        uint32_t getHeight() const { return windowHeight; }

    private:
        SDL_Window* window;
        bool shouldCloseFlag;
        bool resizedFlag;
        uint32_t windowWidth, windowHeight;
    };
}
