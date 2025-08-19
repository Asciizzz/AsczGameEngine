#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "AzVulk/Buffer.hpp"
#include "AzVulk/Device.hpp"

namespace Az3D {

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 nrml;
        glm::vec2 txtr;

        Vertex() : pos(0.0f), nrml(0.0f), txtr(0.0f) {}
        Vertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& texCoord)
            : pos(position), nrml(normal), txtr(texCoord) {}

        // Vulkan binding description for rendering
        static VkVertexInputBindingDescription getBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
    };

    // Transform structure
    struct Transform {
        glm::vec3 pos{0.0f};
        glm::quat rot{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scl{1.0f};

        void translate(const glm::vec3& translation);
        void rotate(const glm::quat& rotation);
        void rotateX(float radians);
        void rotateY(float radians);
        void rotateZ(float radians);
        void scale(float scale);
        void scale(const glm::vec3& scale);

        glm::mat4 getMat4() const;
        void reset();

        // Useful static methods

        static glm::vec3 rotate(const glm::vec3& point, const glm::vec3& axis, float angle) {
            glm::quat rotation = glm::angleAxis(angle, axis);
            return rotation * point;
        }
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

        // Delete copy semantics
        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        Mesh() = default;
        Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices)
            : vertices(std::move(vertices)), indices(std::move(indices)) {}

        // Mesh data
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        static SharedPtr<Mesh> loadFromOBJ(std::string filePath);

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
        HitInfo closestHit(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, const Transform& transform) const;
        HitInfo closestHit(const glm::vec3& center, float radius, const Transform& transform) const;

        // Helper methods for BVH (implemented in Mesh_BVH.cpp)
        static float rayIntersectBox(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& boxMin, const glm::vec3& boxMax);
        static glm::vec3 rayIntersectTriangle(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);
        static float sphereIntersectBox(const glm::vec3& sphereOrigin, float sphereRadius, const glm::vec3& boxMin, const glm::vec3& boxMax);
        static glm::vec3 sphereIntersectTriangle(const glm::vec3& sphereOrigin, float sphereRadius, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);
    };


    class MeshManager {
    public:
        MeshManager(AzVulk::Device& vkDevice);
        MeshManager(const MeshManager&) = delete;
        MeshManager& operator=(const MeshManager&) = delete;

        size_t addMesh(SharedPtr<Mesh> mesh);
        size_t addMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
        size_t loadFromOBJ(std::string filePath);

        // Index-based mesh storage
        size_t count = 0; // Track the number of meshes
        SharedPtrVec<Mesh> meshes;

        AzVulk::Device& vkDevice;

        std::vector<AzVulk::BufferData> vertexBufferDatas;
        std::vector<AzVulk::BufferData> indexBufferDatas;
        void createBufferDatas();
    };
}
