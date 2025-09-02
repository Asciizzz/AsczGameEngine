#include "Az3D/RigSkeleton.hpp"
#include <iostream>

using namespace Az3D;


void RigSkeleton::debugPrintHierarchy() const {
    for (size_t i = 0; i < names.size(); ++i) {
        if (parentIndices[i] == -1) {
            debugPrintRecursive(static_cast<int>(i), 0);
        }
    }
}

void RigSkeleton::debugPrintRecursive(int boneIndex, int depth) const {
    // Indentation
    for (int i = 0; i < depth; ++i) std::cout << "| ";

    // Print this bone
    std::cout << "| " << names[boneIndex] << " (index " << boneIndex << ")";
    if (parentIndices[boneIndex] != -1) {
        std::cout << " [parent " << parentIndices[boneIndex] << "]";
    }
    std::cout << "\n";

    // Recurse into children
    for (size_t i = 0; i < names.size(); ++i) {
        if (parentIndices[i] == boneIndex) {
            debugPrintRecursive(static_cast<int>(i), depth + 1);
        }
    }
}