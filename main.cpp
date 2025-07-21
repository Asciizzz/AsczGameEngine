#define SDL_MAIN_HANDLED

#include <AzVulk/Pipeline.h>
#include <AzVulk/SwapChain.h>

#include <SDL2/SDL_keyboard.h>

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
};

int main() {
    AzVulk::Device device("Hi", 800, 600);
    AzVulk::SwapChain swapchain(device, {800, 600});
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

    bool usePipeline1 = true;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            running = event.type != SDL_QUIT;
        }
        if (!running) break;

        // swapchain.drawFrame(pipeline1.getGraphicsPipeline());
        if (usePipeline1)
            swapchain.drawFrame(
                pipeline1.getGraphicsPipeline(),
                model.getVertexBuffer(), vertices,
                model.getIndexBuffer(), indices
            );
        else
            swapchain.drawFrame(
                pipeline2.getGraphicsPipeline(),
                model.getVertexBuffer(), vertices,
                model.getIndexBuffer(), indices
            );

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

    swapchain.cleanup();

    pipeline1.cleanup();
    pipeline2.cleanup();

    model.cleanup();

    device.cleanup();

    printf("\n\n\n\033[1;32mBye, Vulkan!\033[0m\n\n\n");

    return EXIT_SUCCESS;
}