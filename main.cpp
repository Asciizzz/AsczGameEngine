#define SDL_MAIN_HANDLED
#include "AzCore/Application.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        Application app("AzVulk Engine", 1600, 900);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Application error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}