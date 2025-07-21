#define SDL_MAIN_HANDLED

#include <AzVulk/Pipeline.hpp>
#include <AzVulk/SwapChain.hpp>

#include <SDL2/SDL_keyboard.h>

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
};

int main() {
    AzVulk::Device device("Hi", 800, 600);
    AzVulk::SwapChain swapchain(device, {800, 600});
    AzVulk::Pipeline pipeline(
        device,
        "Shaders/hello.vert.spv",
        "Shaders/hello.frag.spv",
        swapchain.getRenderPass()
    );

    const std::vector<AzVulk::Vertex> vertices = {
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
    };
    const std::vector<uint32_t> indices = {
        0, 1, 2
    };

    AzVulk::Model model(device, vertices, indices);

    bool holdQ = false;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            running = event.type != SDL_QUIT;
        }
        if (!running) break;

        swapchain.drawFrame(
            pipeline.getGraphicsPipeline(),
            model.getVertexBuffer(), vertices,
            model.getIndexBuffer(), indices
        );

        // Press Q to print a message
        const Uint8* state = SDL_GetKeyboardState(nullptr);
        if (state[SDL_SCANCODE_Q] && !holdQ) {
            holdQ = true;
            printf("\033[1;32mHello, Vulkan!\033[0m\n");
        }
        if (!state[SDL_SCANCODE_Q]) {
            holdQ = false;
        }
    }

    vkDeviceWaitIdle(device.getDevice());

    swapchain.cleanup();

    pipeline.cleanup();

    model.cleanup();

    device.cleanup();

    printf("\n\n\n\033[1;32mBye, Vulkan!\033[0m\n\n\n");

    return EXIT_SUCCESS;
}