#pragma once

#include <vulkan/vulkan.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <vector>
#include <array>

namespace Az3D {
    // Vertex structure moved from AzVulk to Az3D
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord;

        // Vulkan binding description for rendering
        static VkVertexInputBindingDescription getBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
    };

    // Clean mesh class - contains only vertices and indices
    class Mesh {
    public:
        Mesh() = default;
        Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
        Mesh(std::vector<Vertex>&& vertices, std::vector<uint32_t>&& indices);
        
        // Move constructor and assignment
        Mesh(Mesh&& other) noexcept;
        Mesh& operator=(Mesh&& other) noexcept;
        
        // Delete copy constructor and assignment (use shared_ptr for sharing)
        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        // Getters
        const std::vector<Vertex>& getVertices() const { return vertices; }
        const std::vector<uint32_t>& getIndices() const { return indices; }
        
        // Setters
        void setVertices(const std::vector<Vertex>& vertices);
        void setVertices(std::vector<Vertex>&& vertices);
        void setIndices(const std::vector<uint32_t>& indices);
        void setIndices(std::vector<uint32_t>&& indices);
        
        // Clear data
        void clear();
        
        // Check if mesh has data
        bool isEmpty() const;

    private:
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };
}
