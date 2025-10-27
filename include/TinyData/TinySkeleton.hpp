#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

struct TinyBone {
    std::string name;

    int parent = -1; // -1 if root
    std::vector<int> children;

    glm::mat4 bindInverse = glm::mat4(1.0f); // From mesh space to bone local space
    glm::mat4 bindPose = glm::mat4(1.0f); // Local transform in bind pose
};

struct TinySkeleton {
    TinySkeleton() = default;

    TinySkeleton(const TinySkeleton&) = delete;
    TinySkeleton& operator=(const TinySkeleton&) = delete;

    TinySkeleton(TinySkeleton&&) = default;
    TinySkeleton& operator=(TinySkeleton&&) = default;

    std::string name;
    std::vector<TinyBone> bones;

    void clear() { 
        name.clear();
        bones.clear(); 
    }
    uint32_t insert(const TinyBone& bone) {
        bones.push_back(bone);
        return static_cast<uint32_t>(bones.size() - 1);
    }
};