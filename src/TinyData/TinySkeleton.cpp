#include "TinyData/TinySkeleton.hpp"
#include <iostream>
#include <iomanip>

void TinySkeleton::clear() {
    names.clear();
    parents.clear();
    inverseBindMatrices.clear();
    localBindTransforms.clear();
    nameToIndex.clear();
}

void TinySkeleton::insert(const TinyJoint& joint) {
    int index = static_cast<int>(names.size());
    nameToIndex[joint.name] = index;

    names.push_back(joint.name);
    parents.push_back(joint.parent);
    inverseBindMatrices.push_back(joint.inverseBindMatrix);
    localBindTransforms.push_back(joint.localBindTransform);
}

void TinySkeleton::debugPrintHierarchy() const {
    std::cout << "Skeleton Hierarchy (" << names.size() << " bones):\n";
    std::cout << std::string(50, '=') << "\n";
    
    // Find root bones (those with parent index -1)
    for (int i = 0; i < static_cast<int>(names.size()); ++i) {
        if (parents[i] == -1) {
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
    
    if (parents[boneIndex] != -1) {
        std::cout << " (parent: " << parents[boneIndex] << ")";
    } else {
        std::cout << " (root)";
    }
    std::cout << "\n";
    
    // Print children
    for (int i = 0; i < static_cast<int>(names.size()); ++i) {
        if (parents[i] == boneIndex) {
            debugPrintRecursive(i, depth + 1);
        }
    }
}