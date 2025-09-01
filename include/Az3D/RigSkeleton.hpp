#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Az3D {

struct RigSkeleton {
    // Bone SoA
    std::vector<std::string> names;
    std::vector<int> parentIndices;
    std::vector<glm::mat4> inverseBindMatrices;
    std::vector<glm::mat4> localBindTransforms;

    std::unordered_map<std::string, int> nameToIndex;

    std::vector<glm::mat4> computeGlobalTransforms(const std::vector<glm::mat4>& localPoseTransforms) const;
    std::vector<glm::mat4> copyLocalBindToPoseTransforms() const;

    void debugPrintHierarchy() const;
    void debugPrintRecursive(int boneIndex, int depth) const;
};

} // namespace Az3D
