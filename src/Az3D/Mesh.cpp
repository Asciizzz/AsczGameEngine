#include "Az3D/Mesh.hpp"

namespace Az3D {
    // Vertex static methods for Vulkan compatibility
    VkVertexInputBindingDescription Vertex::getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    std::array<VkVertexInputAttributeDescription, 3> Vertex::getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        // Position attribute (location 0)
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, position);

        // Normal attribute (location 1)
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, normal);

        // Texture coordinate attribute (location 2)
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }

    // Mesh implementation
    Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
        : vertices(vertices), indices(indices) {
    }

    Mesh::Mesh(std::vector<Vertex>&& vertices, std::vector<uint32_t>&& indices)
        : vertices(std::move(vertices)), indices(std::move(indices)) {
    }

    Mesh::Mesh(Mesh&& other) noexcept
        : vertices(std::move(other.vertices)), indices(std::move(other.indices)) {
    }

    Mesh& Mesh::operator=(Mesh&& other) noexcept {
        if (this != &other) {
            vertices = std::move(other.vertices);
            indices = std::move(other.indices);
        }
        return *this;
    }

    void Mesh::setVertices(const std::vector<Vertex>& vertices) {
        this->vertices = vertices;
    }

    void Mesh::setVertices(std::vector<Vertex>&& vertices) {
        this->vertices = std::move(vertices);
    }

    void Mesh::setIndices(const std::vector<uint32_t>& indices) {
        this->indices = indices;
    }

    void Mesh::setIndices(std::vector<uint32_t>&& indices) {
        this->indices = std::move(indices);
    }

    void Mesh::clear() {
        vertices.clear();
        indices.clear();
    }

    bool Mesh::isEmpty() const {
        return vertices.empty() || indices.empty();
    }
}
