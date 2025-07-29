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
        Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices, bool hasBVH = false)
            : vertices(std::move(vertices)), indices(std::move(indices)) {}

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        static std::shared_ptr<Mesh> loadFromOBJ(const char* filePath, bool hasBVH = false);

    };


    class MeshManager {
    public:
        MeshManager() = default;
        MeshManager(const MeshManager&) = delete;
        MeshManager& operator=(const MeshManager&) = delete;

        size_t addMesh(std::shared_ptr<Mesh> mesh);
        size_t addMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
        size_t loadMeshFromOBJ(const char* filePath);

        // Index-based mesh storage
        std::vector<std::shared_ptr<Mesh>> meshes;
    };
}
