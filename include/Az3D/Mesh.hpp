#pragma once

#include <vulkan/vulkan.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <memory>
#include <string>

namespace Az3D {

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 nrml;
        glm::vec2 txtr;

        // Vulkan binding description for rendering
        static VkVertexInputBindingDescription getBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
    };


    struct Mesh {
        Mesh() = default;
        Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices)
            : vertices(std::move(vertices)), indices(std::move(indices)) {}

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        bool isEmpty();
        static std::shared_ptr<Mesh> loadFromOBJ(const char* filePath);
    };


    class MeshManager {
    public:
        MeshManager() = default;
        
        // Delete copy constructor and assignment operator
        MeshManager(const MeshManager&) = delete;
        MeshManager& operator=(const MeshManager&) = delete;

        // Index-based mesh management
        size_t addMesh(std::shared_ptr<Mesh> mesh);
        bool removeMesh(size_t index);
        bool hasMesh(size_t index) const;
        Mesh* getMesh(size_t index) const;
        
        // Load mesh and return index
        size_t loadMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
        size_t loadMeshFromOBJ(const char* filePath);
        
        // Create simple meshes and return index
        size_t createQuadMesh();
        size_t createCubeMesh();

        // Index-based mesh storage
        std::vector<std::shared_ptr<Mesh>> meshes;
    };
}
