#include "Az3D/TinyModel.hpp"
#include <iostream>
#include <iomanip>

using namespace Az3D;

// ============================================================================
// MATERIAL IMPLEMENTATIONS
// ============================================================================

void TinyMaterial::setShadingParams(bool shading, int toonLevel, float normalBlend, float discardThreshold) {
    shadingParams.x = shading ? 1.0f : 0.0f;
    shadingParams.y = static_cast<float>(toonLevel);
    shadingParams.z = normalBlend;
    shadingParams.w = discardThreshold;
}

void TinyMaterial::setAlbedoTexture(int index, TAddressMode addressMode) {
    texIndices.x = index;
    texIndices.y = static_cast<uint32_t>(addressMode);
}

void TinyMaterial::setNormalTexture(int index, TAddressMode addressMode) {
    texIndices.z = index;
    texIndices.w = static_cast<uint32_t>(addressMode);
}

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
    std::cout << "[" << std::setw(2) << boneIndex << "] " 
              << names[boneIndex];
    
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