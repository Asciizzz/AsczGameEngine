#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

struct tinyBone {
    std::string name;

    int parent = -1; // -1 if root
    std::vector<int> children;

    glm::mat4 bindInverse = glm::mat4(1.0f); // From mesh space to bone local space
    glm::mat4 bindPose = glm::mat4(1.0f); // Local transform in bind pose
};

struct tinySkeleton {
    tinySkeleton() = default;

    tinySkeleton(const tinySkeleton&) = delete;
    tinySkeleton& operator=(const tinySkeleton&) = delete;

    tinySkeleton(tinySkeleton&&) = default;
    tinySkeleton& operator=(tinySkeleton&&) = default;

    std::string name;
    std::vector<tinyBone> bones;

    void clear() { 
        name.clear();
        bones.clear(); 
    }
    uint32_t insert(const tinyBone& bone) {
        bones.push_back(bone);
        return static_cast<uint32_t>(bones.size() - 1);
    }
};