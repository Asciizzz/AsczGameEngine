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

        Mesh(Mesh&& other) noexcept;
        Mesh& operator=(Mesh&& other) noexcept;
        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        bool isEmpty();

        // Static factory method for loading OBJ files
        static std::shared_ptr<Mesh> loadFromOBJ(const std::string& filePath);

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };
}
