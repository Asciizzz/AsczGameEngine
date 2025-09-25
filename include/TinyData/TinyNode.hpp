#pragma once

#include <vector>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct TinyNode {
    uint32_t id = 0;                 // unique ID if needed
    int parent = -1;                 // parent index, -1 if root
    std::vector<uint32_t> children;  // children indices

    glm::mat4 localTransform = glm::mat4(1.0f);
    glm::mat4 worldTransform = glm::mat4(1.0f);

    uint32_t meshId = UINT32_MAX;     // index into local mesh
    uint32_t materialId = UINT32_MAX; // index into local material
};