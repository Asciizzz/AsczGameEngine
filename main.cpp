#define SDL_MAIN_HANDLED
#include "tinyApp/tinyApp.hpp"

#include <iostream>
#include <stdexcept>

int main() {

    try {
        // Peak 2009 window engineering
        tinyApp app("AszcEngine", 1600, 900);
        app.run();
    } catch (const std::exception& e) {
        // Plot twist: things broke
        std::cerr << "Application error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    // Miraculously, nothing exploded
    return EXIT_SUCCESS;
};