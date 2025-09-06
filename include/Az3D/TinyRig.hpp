#pragma once

#include <Az3D/VertexTypes.hpp>

namespace Az3D {

struct TinySkeleton {
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

}