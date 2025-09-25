#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>

#include "Helpers/Templates.hpp"

struct TinyJoint {
    int parent = -1; // -1 if root
    std::string name;

    glm::mat4 inverseBindMatrix = glm::mat4(1.0f); // From mesh space to bone local space
    glm::mat4 localBindTransform = glm::mat4(1.0f); // Local transform in bind pose
};

struct TinySkeleton {
    // Joint SoA
    std::vector<std::string> names;
    std::vector<int> parents;
    std::vector<glm::mat4> inverseBindMatrices;
    std::vector<glm::mat4> localBindTransforms;

    std::unordered_map<std::string, int> nameToIndex;

    void clear();
    void insert(const TinyJoint& joint);

    void debugPrintHierarchy() const;
private:
    void debugPrintRecursive(int boneIndex, int depth) const;
};