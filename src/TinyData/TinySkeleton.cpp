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

void TinySkeleton::insert(const TinyBone& bone) {
    int index = static_cast<int>(names.size());
    nameToIndex[bone.name] = index;

    names.push_back(bone.name);
    parents.push_back(bone.parent);
    inverseBindMatrices.push_back(bone.inverseBindMatrix);
    localBindTransforms.push_back(bone.localBindTransform);
}

std::vector<TinyBone> TinySkeleton::construct() const {
    std::vector<TinyBone> bones;
    bones.reserve(names.size());

    for (int i = 0; i < static_cast<int>(names.size()); ++i) {
        TinyBone bone;
        bone.name = names[i];
        bone.parent = parents[i];
        bone.inverseBindMatrix = inverseBindMatrices[i];
        bone.localBindTransform = localBindTransforms[i];
        bones.push_back(std::move(bone));
    }

    // Build children lists
    for (int i = 0; i < static_cast<int>(bones.size()); ++i) {
        int parentIndex = bones[i].parent;
        if (parentIndex >= 0 && parentIndex < static_cast<int>(bones.size())) {
            bones[parentIndex].children.push_back(i);
        }
    }

    return bones;
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