#pragma once

#include <vulkan/vulkan.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <array>
#include <memory>
#include <string>
#include <algorithm>
#include <iostream>
#include <queue>

namespace Az3D {

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 nrml;
        glm::vec2 txtr;

        // Vulkan binding description for rendering
        static VkVertexInputBindingDescription getBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
    };

    // Transform structure
    struct Transform {
        glm::vec3 pos{0.0f};
        glm::quat rot{1.0f, 0.0f, 0.0f, 0.0f};
        float scl{1.0f};

        void translate(const glm::vec3& translation);
        void rotate(const glm::quat& rotation);
        void rotateX(float radians);
        void rotateY(float radians);
        void rotateZ(float radians);
        void scale(float scale);

        // Legacy methods for compatibility
        void rotate(const glm::vec3& eulerAngles);

        glm::mat4 modelMatrix() const;
        void reset();
    };

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
        glm::vec3 pos{}; // Keep both for compatibility  
        glm::vec3 nrml = glm::vec3(0.0f);
        glm::vec3 normal{}; // Keep both for compatibility
        uint32_t materialId = 0;
    };

    struct Mesh {
        static constexpr size_t MAX_DEPTH = 32;
        static constexpr size_t BIN_COUNT = 11;

        Mesh() = default;
        Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices, bool hasBVH = false)
            : vertices(std::move(vertices)), indices(std::move(indices)), useBVH(hasBVH) {
            if (hasBVH) { createBVH(); }
        }

        // Mesh data
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        // BVH data structures
        glm::vec3 meshMin = glm::vec3(FLT_MAX);
        glm::vec3 meshMax = glm::vec3(-FLT_MAX);

        bool useBVH = false;
        std::vector<BVHNode> nodes;
        std::vector<size_t> sortedIndices; // For BVH traversal
        std::vector<glm::vec3> unsortedABmin;
        std::vector<glm::vec3> unsortedABmax;
        std::vector<glm::vec3> unsortedCenters;
        size_t indexCount = 0; // Number of indices in the mesh

        // BVH methods (implemented in Mesh_BVH.cpp)
        void createBVH();
        void buildBVH();
        HitInfo closestHit(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, const Transform& transform) const;
        HitInfo closestHit(const glm::vec3& center, float radius, const Transform& transform) const;

        // Helper methods for BVH (implemented in Mesh_BVH.cpp)
        static float rayIntersectBox(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& boxMin, const glm::vec3& boxMax);
        static glm::vec3 rayIntersectTriangle(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);
        static float sphereIntersectBox(const glm::vec3& sphereOrigin, float sphereRadius, const glm::vec3& boxMin, const glm::vec3& boxMax);
        static glm::vec3 sphereIntersectTriangle(const glm::vec3& sphereOrigin, float sphereRadius, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);

        static std::shared_ptr<Mesh> loadFromOBJ(const char* filePath, bool hasBVH = false);
    };


    class MeshManager {
    public:
        MeshManager() = default;
        MeshManager(const MeshManager&) = delete;
        MeshManager& operator=(const MeshManager&) = delete;

        size_t addMesh(std::shared_ptr<Mesh> mesh);
        size_t addMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
        size_t loadMeshFromOBJ(const char* filePath, bool hasBVH = false);

        // Index-based mesh storage
        std::vector<std::shared_ptr<Mesh>> meshes;
    };
}
