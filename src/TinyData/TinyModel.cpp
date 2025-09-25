#include "TinyData/TinyModel.hpp"
#include <iostream>
#include <iomanip>


void TinyModel::printAnimationList() const {
    // We just need to print the names of the animations
    std::cout << "Animations: " << animations.size() << "\n";
    std::cout << std::string(30, '-') << "\n";
    for (size_t i = 0; i < animations.size(); ++i) {
        const auto& anim = animations[i];
        std::cout << "  Animation[" << i << "]: " << anim.name 
                  << " (duration: " << std::fixed << std::setprecision(2) 
                  << anim.duration << "s, channels: " << anim.channels.size() 
                  << ", samplers: " << anim.samplers.size() << ")\n";
    }
}