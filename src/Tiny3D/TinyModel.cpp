#include "Tiny3D/TinyModel.hpp"
#include <iostream>
#include <iomanip>

uint32_t TinyModel::getSubmeshIndexCount(size_t index) const {
    if (index >= submeshes.size()) return 0;
    return static_cast<uint32_t>(submeshes[index].indices.size());
}

void TinyModel::printDebug() const {
    std::cout << "Tiny3D/TinyModel Information\n";
    std::cout << std::string(50, '=') << "\n";
    
    // Print mesh information
    std::cout << "Meshes: " << submeshes.size() << "\n";
    std::cout << std::string(30, '-') << "\n";
    for (size_t i = 0; i < submeshes.size(); ++i) {
        const auto& mesh = submeshes[i];
        std::cout << "  Mesh[" << i << "]: "
                  << mesh.vertexCount() << " verts, "
                  << mesh.indices.size() << " idxs, "
                  << "matIdx: : " << mesh.matIndex << "\n";
    }

    // Print material information
    std::cout << "\nMaterials: " << materials.size() << "\n";
    std::cout << std::string(30, '-') << "\n";
    for (size_t i = 0; i < materials.size(); ++i) {
        const auto& material = materials[i];
        std::cout << "  Material[" << i << "]: "
                  << "albIdx: " << material.albTexture << ", "
                  << "nrmlIdx: " << material.nrmlTexture << "\n";
    }
    
    // Print texture information
    std::cout << "\nTextures: " << textures.size() << "\n";
    std::cout << std::string(30, '-') << "\n";
    for (size_t i = 0; i < textures.size(); ++i) {
        const auto& texture = textures[i];
        std::cout << "  Texture[" << i << "]: "
                  << texture.width << "x" << texture.height 
                  << " (" << texture.channels << " channels)\n";
    }
    
    // Print skeleton information
    std::cout << "\nSkeleton: " << skeleton.names.size() << " bones\n";
    std::cout << std::string(30, '-') << "\n";
    if (!skeleton.names.empty()) {
        std::cout << "  Bones: ";
        for (size_t i = 0; i < std::min(size_t(5), skeleton.names.size()); ++i) {
            std::cout << skeleton.names[i];
            if (i < std::min(size_t(5), skeleton.names.size()) - 1) std::cout << ", ";
        }
        if (skeleton.names.size() > 5) {
            std::cout << "... (+" << (skeleton.names.size() - 5) << " more)";
        }
        std::cout << "\n";
    } else {
        std::cout << "  No skeleton data\n";
    }
    
    std::cout << std::string(60, '=') << "\n";
}



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