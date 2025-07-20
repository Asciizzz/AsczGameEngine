#define SDL_MAIN_HANDLED

#include <AzVulk/Device.h>
#include <AzVulk/Pipeline.h>
#include <AzVulk/SwapChain.h>

#include <glm/glm.hpp>

#include <SDL2/SDL_keyboard.h>

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
};

int main() {
    AzVulk::Device device(800, 600);
    AzVulk::SwapChain swapChain(device);
    AzVulk::Pipeline pipeline(
        device, swapChain, 
        "Shaders/hello.vert.spv", "Shaders/hello.frag.spv"
    );

    const std::vector<Vertex> vertices = {
        {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
    };


    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                return 0;
            }
        }

        // app.drawFrame(pipeline.getGraphicsPipeline());
        swapChain.drawFrame(pipeline.getGraphicsPipeline());

        // Press Q to print a message 
        const Uint8* state = SDL_GetKeyboardState(nullptr);
        if (state[SDL_SCANCODE_Q]) {
            std::cout << "Q key pressed!" << std::endl;
        }
    }

    vkDeviceWaitIdle(device.getDevice());

    // pipeline.cleanup();
    // swapchain.cleanup();
    device.cleanup();

    return EXIT_SUCCESS;
}