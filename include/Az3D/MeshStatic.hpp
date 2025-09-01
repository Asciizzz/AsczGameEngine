#pragma once

#include "Az3D/VertexTypes.hpp"

namespace Az3D {

// BVH structures
struct BVHNode {
    glm::vec3 min;
    glm::vec3 max;

    /* Note:
    -1 children means leaf
    Leaf range is [l_leaf, r_leaf)
    */
    int l_child = -1;
    int r_child = -1;
    size_t l_leaf = 0;
    size_t r_leaf = 0;
};

struct HitInfo {
    bool hit = false;
    bool hasHit = false; // Keep both for compatibility
    size_t index = 0;
    uint32_t triangleIndex = 0; // Keep both for compatibility
    glm::vec3 prop = glm::vec3(-1.0f); // {u, v, t} (u, v are for barycentric coordinates, t is distance)
    glm::vec3 barycentric{}; // Keep both for compatibility
    float t = 1e30f; // Keep both for compatibility
    
    // Get the vertex and normal at the hit point
    glm::vec3 vrtx = glm::vec3(0.0f);
    glm::vec3 nrml = glm::vec3(0.0f);
    uint32_t materialId = 0;
};

struct MeshStatic {
    static constexpr size_t MAX_DEPTH = 32;
    static constexpr size_t BIN_COUNT = 11;

    // Delete copy semantics
    MeshStatic(const MeshStatic&) = delete;
    MeshStatic& operator=(const MeshStatic&) = delete;

    MeshStatic() = default;
    MeshStatic(std::vector<VertexStatic> vertices, std::vector<uint32_t> indices)
        : vertices(std::move(vertices)), indices(std::move(indices)) {}

    // Mesh data
    std::vector<VertexStatic> vertices;
    std::vector<uint32_t> indices;

    // BVH data structures
    glm::vec3 meshMin = glm::vec3(FLT_MAX);
    glm::vec3 meshMax = glm::vec3(-FLT_MAX);

    bool hasBVH = false;
    std::vector<BVHNode> nodes;
    std::vector<size_t> sortedIndices; // For BVH traversal
    std::vector<glm::vec3> unsortedABmin;
    std::vector<glm::vec3> unsortedABmax;
    std::vector<glm::vec3> unsortedCenters;
    size_t indexCount = 0; // Number of indices in the mesh

    // BVH methods (implemented in Mesh_BVH.cpp)
    void createBVH();
    void buildBVH();
    HitInfo closestHit(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, const glm::mat4& modelMat4) const;
    HitInfo closestHit(const glm::vec3& center, float radius, const glm::mat4& modelMat4) const;

    // Helper methods for BVH (implemented in Mesh_BVH.cpp)
    static float rayIntersectBox(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& boxMin, const glm::vec3& boxMax);
    static glm::vec3 rayIntersectTriangle(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);
    static float sphereIntersectBox(const glm::vec3& sphereOrigin, float sphereRadius, const glm::vec3& boxMin, const glm::vec3& boxMax);
    static glm::vec3 sphereIntersectTriangle(const glm::vec3& sphereOrigin, float sphereRadius, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);
};

}
