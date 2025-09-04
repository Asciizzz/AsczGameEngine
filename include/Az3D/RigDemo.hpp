#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Az3D {

// ============================================================================
// RIG SKELETON DEMO STRUCTURE
// ============================================================================

struct RigDemo {
    // Bone SoA
    std::vector<std::string> names;
    std::vector<int> parentIndices;
    std::vector<glm::mat4> inverseBindMatrices;
    std::vector<glm::mat4> localBindTransforms;

    std::unordered_map<std::string, int> nameToIndex;

    void debugPrintHierarchy() const;

private:
    void debugPrintRecursive(int boneIndex, int depth) const;
};

} // namespace Az3D
