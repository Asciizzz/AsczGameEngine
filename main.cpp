#define SDL_MAIN_HANDLED
#include "AzVulk/Application.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        AzVulk::Application app("AzVulk Engine", 800, 600);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Application error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}