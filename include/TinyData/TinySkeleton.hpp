#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>

#include ".ext/Templates.hpp"

struct TinyBone {
    std::string name;

    int parent = -1; // -1 if root
    std::vector<int> children;

    glm::mat4 inverseBindMatrix = glm::mat4(1.0f); // From mesh space to bone local space
    glm::mat4 localBindTransform = glm::mat4(1.0f); // Local transform in bind pose
};

struct TinySkeleton {
    std::string name;
    std::vector<TinyBone> bones;

    void clear() { 
        name.clear();
        bones.clear(); 
    }
    uint32_t insert(const TinyBone& bone);

    void debugPrintHierarchy() const;
private:
    void debugPrintRecursive(int boneIndex, int depth) const;
};