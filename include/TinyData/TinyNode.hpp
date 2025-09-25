#pragma once

#include <vector>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "TinyData/TinyEnum.hpp"

struct TinyNode {
    TinyScope scope = TinyScope::Local;

    int parent = -1; // -1 if root
    std::vector<uint32_t> children;

    glm::mat4 transform = glm::mat4(1.0f);

    uint32_t meshId = UINT32_MAX;
    std::vector<uint32_t> matIds;
    uint32_t skeleId = UINT32_MAX;
    uint32_t animId = UINT32_MAX;
};