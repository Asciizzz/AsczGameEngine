#define SDL_MAIN_HANDLED

#include <AzVulk.h>

#include <SDL2/SDL_keyboard.h>


int main() {
    AzVulk app(800, 600);

    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                return 0;
            }
        }

        SDL_Delay(16); // Approx 60 FPS

        // Press Q to print a message 
        const Uint8* state = SDL_GetKeyboardState(nullptr);
        if (state[SDL_SCANCODE_Q]) {
            std::cout << "Q key pressed!" << std::endl;
        }
    }

    return EXIT_SUCCESS;
}