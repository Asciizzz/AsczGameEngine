#include "Az3D/Model.hpp"
#include <iostream>
#include <iomanip>

namespace Az3D {

void Skeleton::debugPrintHierarchy() const {
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

void Skeleton::debugPrintRecursive(int boneIndex, int depth) const {
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

} // namespace Az3D
