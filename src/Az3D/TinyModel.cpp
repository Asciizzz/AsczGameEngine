#include "Az3D/TinyModel.hpp"
#include <iostream>
#include <iomanip>

using namespace Az3D;

// ============================================================================
// SKELETON IMPLEMENTATIONS
// ============================================================================

void TinySkeleton::debugPrintHierarchy() const {
    std::cout << "Skeleton Hierarchy (" << names.size() << " bones):\n";
    std::cout << std::string(50, '=') << "\n";
    
    // Find root bones (those with parent index -1)
    for (int i = 0; i < static_cast<int>(names.size()); ++i) {
        if (parentIndices[i] == -1) {
            debugPrintRecursive(i, 0);
        }
    }
    
    std::cout << std::string(50, '=') << "\n";
}

void TinySkeleton::debugPrintRecursive(int boneIndex, int depth) const {
    if (boneIndex < 0 || boneIndex >= static_cast<int>(names.size())) {
        return;
    }
    
    // Print indentation
    for (int i = 0; i < depth; ++i) {
        std::cout << "  ";
    }
    
    // Print bone info
    std::cout << "[" << boneIndex << "] " << names[boneIndex];
    
    if (parentIndices[boneIndex] != -1) {
        std::cout << " (parent: " << parentIndices[boneIndex] << ")";
    } else {
        std::cout << " (root)";
    }
    std::cout << "\n";
    
    // Print children
    for (int i = 0; i < static_cast<int>(names.size()); ++i) {
        if (parentIndices[i] == boneIndex) {
            debugPrintRecursive(i, depth + 1);
        }
    }
}

// ============================================================================
// MODEL IMPLEMENTATIONS
// ============================================================================

uint32_t TinyModel::getSubmeshIndexCount(size_t index) const {
    if (index >= submeshes.size()) return 0;
    return static_cast<uint32_t>(submeshes[index].indices.size());
}

void TinyModel::printDebug() const {
    std::cout << "TinyModel Information\n";
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