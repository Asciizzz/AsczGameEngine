#define SDL_MAIN_HANDLED

#include <AzVulk/Instance.h>
#include <AzVulk/Pipeline.h>

#include <AzVulk/Window.h>

#include <SDL2/SDL_Keyboard.h>

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);

    AzVulk::Instance vk_instance;
    AzVulk::Pipeline vk_pipeline("Shaders/simple.vert.spv", "Shaders/simple.frag.spv");

    AzVulk::Window vk_window(800, 600, "SDL Window");

    int q_count = 0;
    bool q_down = false;

    while (true) {
        // Poll for events
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                // Handle quit event
                printf("Quit event received. Exiting...\n");
                return 0;
            }
        }

        // Press Q to print a message
        const Uint8* state = SDL_GetKeyboardState(NULL);
        if (state[SDL_SCANCODE_Q] && !q_down) {
            q_count++;
            printf("Q key pressed %d times.\n", q_count);
            q_down = true;
        }
        if (!state[SDL_SCANCODE_Q]) {
            q_down = false;
        }
    }

    return 0;
}