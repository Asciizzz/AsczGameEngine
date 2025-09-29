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

    int meshIndex = -1;
    std::vector<int> meshMaterialIndices;
    int skeletonIndex = -1;

    void printDebug(uint32_t index) const {
        // Print with red color for title
        printf("\033[31mNode %d: %s\033[0m\n", index, name.c_str());
        printf("  Parent: %d\n", parent);
        printf("  Children: ");
        for (int child : children) {
            printf("%d ", child);
        }
        printf("\n");
        printf("  Mesh Index: %d\n", meshIndex);
        printf("  Skeleton Index: %d\n", skeletonIndex);
        printf("  Transform:\n");
        for (int i = 0; i < 4; ++i) {
            printf("    %.3f %.3f %.3f %.3f\n", transform[i][0], transform[i][1], transform[i][2], transform[i][3]);
        }
    }
};