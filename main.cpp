#define SDL_MAIN_HANDLED

#include <AzVulk/Pipeline.h>
#include <AzVulk/SwapChain.h>

#include <SDL2/SDL_keyboard.h>

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
};

int main() {
    AzVulk::Device device(800, 600);
    AzVulk::SwapChain swapchain(device);
    AzVulk::Pipeline pipeline1(
        device, swapchain, 
        "Shaders/hello.vert.spv",
        "Shaders/hello.frag.spv"
    );

    AzVulk::Pipeline pipeline2(
        device, swapchain, 
        "Shaders/hi.vert.spv",
        "Shaders/hi.frag.spv"
    );

    const std::vector<Vertex> vertices = {
        {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
    };

    AzVulk::Model model(device, {
        {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
    });

    bool holdQ = false;

    bool usePipeline1 = true;
    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                return 0;
            }
        }

        // swapchain.drawFrame(pipeline1.getGraphicsPipeline());
        if (usePipeline1) swapchain.drawFrame(pipeline1.getGraphicsPipeline());
        else              swapchain.drawFrame(pipeline2.getGraphicsPipeline());

        // Press Q to print a message
        const Uint8* state = SDL_GetKeyboardState(nullptr);
        if (state[SDL_SCANCODE_Q] && !holdQ) {
            holdQ = true;

            usePipeline1 = !usePipeline1;
        }
        if (!state[SDL_SCANCODE_Q]) {
            holdQ = false;
        }
    }

    vkDeviceWaitIdle(device.getDevice());

    // pipeline.cleanup();
    pipeline1.cleanup();
    pipeline2.cleanup();

    swapchain.cleanup();
    device.cleanup();

    return EXIT_SUCCESS;
}