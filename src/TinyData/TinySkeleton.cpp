#include "TinyData/TinySkeleton.hpp"
#include <iostream>
#include <iomanip>

uint32_t TinySkeleton::insert(const TinyBone& bone) {
    bones.push_back(bone);
    return static_cast<uint32_t>(bones.size() - 1);
}


void TinySkeleton::debugPrintHierarchy() const {
    std::cout << "Skeleton Hierarchy (" << bones.size() << " bones):\n";
    std::cout << std::string(50, '=') << "\n";
    
    // Find root bones (those with parent index -1)
    for (int i = 0; i < static_cast<int>(bones.size()); ++i) {
        if (bones[i].parent == -1) {
            debugPrintRecursive(i, 0);
        }
    }

    std::cout << std::string(50, '=') << "\n";
}

void TinySkeleton::debugPrintRecursive(int boneIndex, int depth) const {
    if (boneIndex < 0 || boneIndex >= static_cast<int>(bones.size())) {
        return;
    }

    const auto& bone = bones[boneIndex];
    
    // Print indentation
    for (int i = 0; i < depth; ++i) {
        std::cout << "  ";
    }
    
    // Print bone info
    std::cout << "[" << boneIndex << "] " << bone.name;

    if (bone.parent != -1) {
        std::cout << " (parent: " << bones[bone.parent].name << ")";
    } else {
        std::cout << " (root)";
    }
    std::cout << "\n";

    for (const auto& childIndex : bone.children) {
        debugPrintRecursive(childIndex, depth + 1);
    }
}