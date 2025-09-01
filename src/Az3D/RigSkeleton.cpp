#include "Az3D/RigSkeleton.hpp"
#include <iostream>

using namespace Az3D;

// Compute all global bone transforms
std::vector<glm::mat4> RigSkeleton::computeGlobalTransforms(const std::vector<glm::mat4>& localPoseTransforms) const {
    std::vector<glm::mat4> globalTransforms(names.size());
    for (size_t i = 0; i < names.size(); i++) {
        int parent = parentIndices[i];
        if (parent == -1) {
            globalTransforms[i] = localPoseTransforms[i];
        } else {
            globalTransforms[i] = globalTransforms[parent] * localPoseTransforms[i];
        }
    }
    return globalTransforms;
}

std::vector<glm::mat4> RigSkeleton::copyLocalBindToPoseTransforms() const {
    return localBindTransforms;
}

void RigSkeleton::debugPrintHierarchy() const {
    for (size_t i = 0; i < names.size(); i++) {
        if (parentIndices[i] == -1) {
            debugPrintRecursive(static_cast<int>(i), 0);
        }
    }
}

void RigSkeleton::debugPrintRecursive(int boneIndex, int depth) const {
    // Indentation
    for (int i = 0; i < depth; i++) std::cout << "  ";

    // Print this bone
    std::cout << "- " << names[boneIndex] << " (index " << boneIndex << ")";
    if (parentIndices[boneIndex] != -1) {
        std::cout << " [parent " << parentIndices[boneIndex] << "]";
    }
    std::cout << "\n";

    // Recurse into children
    for (size_t i = 0; i < names.size(); i++) {
        if (parentIndices[i] == boneIndex) {
            debugPrintRecursive(static_cast<int>(i), depth + 1);
        }
    }
}
