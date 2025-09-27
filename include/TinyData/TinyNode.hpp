#pragma once

#include <vector>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct TinyNode {
    std::string name = "Node";

    int parent = -1;
    std::vector<int> children; // Indices into global node list

    glm::mat4 transform = glm::mat4(1.0f);

    int meshIndex = -1;     // Index into global mesh list
    int skinIndex = -1;     // Index into global skin/skeleton list
};