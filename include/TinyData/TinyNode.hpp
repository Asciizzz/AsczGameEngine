#pragma once

#include <vector>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct TinyNode {
    enum class Scope {
        Local,      // Local to model
        Global      // Global scene node
    } scope = Scope::Local;

    int parent = -1;                 // parent index, -1 if root
    std::vector<uint32_t> children;  // children indices

    glm::mat4 transform = glm::mat4(1.0f);

    uint32_t meshId = UINT32_MAX; // index into local mesh
    std::vector<uint32_t> matIds; // submesh material indices
    uint32_t skeleId = UINT32_MAX; // index into local skeleton
};