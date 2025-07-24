#include "Az3D/Mesh.hpp"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <unordered_map>

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

    bool Mesh::isEmpty() {
        return vertices.empty() || indices.empty();
    }

    // OBJ loader implementation using tiny_obj_loader
    std::shared_ptr<Mesh> Mesh::loadFromOBJ(const std::string& filePath) {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        // Load the OBJ file
        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath.c_str())) {
            std::cerr << "Failed to load OBJ file: " << filePath << std::endl;
            if (!warn.empty()) std::cerr << "Warning: " << warn << std::endl;
            if (!err.empty()) std::cerr << "Error: " << err << std::endl;
            return nullptr;
        }

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::unordered_map<size_t, uint32_t> uniqueVertices{};

        // Simple hash function for vertex deduplication
        auto hashVertex = [](const Vertex& v) -> size_t {
            size_t h1 = std::hash<float>{}(v.position.x);
            size_t h2 = std::hash<float>{}(v.position.y);
            size_t h3 = std::hash<float>{}(v.position.z);
            size_t h4 = std::hash<float>{}(v.texCoord.x);
            size_t h5 = std::hash<float>{}(v.texCoord.y);
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
        };

        bool hasNormals = !attrib.normals.empty();

        // Process all shapes in the OBJ file
        for (const auto& shape : shapes) {
            for (size_t f = 0; f < shape.mesh.indices.size(); f += 3) {
                // Process triangle (3 vertices)
                std::array<Vertex, 3> triangle;
                
                for (int v = 0; v < 3; v++) {
                    const auto& index = shape.mesh.indices[f + v];
                    Vertex& vertex = triangle[v];

                    // Position (required)
                    if (index.vertex_index >= 0) {
                        vertex.position = {
                            attrib.vertices[3 * index.vertex_index + 0],
                            attrib.vertices[3 * index.vertex_index + 1],
                            attrib.vertices[3 * index.vertex_index + 2]
                        };
                    }

                    // Texture coordinates (optional)
                    if (index.texcoord_index >= 0) {
                        vertex.texCoord = {
                            attrib.texcoords[2 * index.texcoord_index + 0],
                            1.0f - attrib.texcoords[2 * index.texcoord_index + 1] // Flip V coordinate
                        };
                    } else {
                        vertex.texCoord = {0.0f, 0.0f}; // Default UV
                    }

                    // Normals (optional)
                    if (hasNormals && index.normal_index >= 0) {
                        vertex.normal = {
                            attrib.normals[3 * index.normal_index + 0],
                            attrib.normals[3 * index.normal_index + 1],
                            attrib.normals[3 * index.normal_index + 2]
                        };
                    }
                }
                
                // Calculate face normal if no normals provided
                if (!hasNormals) {
                    glm::vec3 v0 = triangle[1].position - triangle[0].position;
                    glm::vec3 v1 = triangle[2].position - triangle[0].position;
                    glm::vec3 faceNormal = glm::normalize(glm::cross(v0, v1));
                    
                    triangle[0].normal = faceNormal;
                    triangle[1].normal = faceNormal;
                    triangle[2].normal = faceNormal;
                }

                // Add vertices with deduplication
                for (const auto& vertex : triangle) {
                    size_t vertexHash = hashVertex(vertex);
                    auto it = uniqueVertices.find(vertexHash);
                    if (it == uniqueVertices.end()) {
                        uniqueVertices[vertexHash] = static_cast<uint32_t>(vertices.size());
                        vertices.push_back(vertex);
                        indices.push_back(static_cast<uint32_t>(vertices.size() - 1));
                    } else {
                        indices.push_back(it->second);
                    }
                }
            }
        }

        return std::make_shared<Mesh>(std::move(vertices), std::move(indices));
    }
}
