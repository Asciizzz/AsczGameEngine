#include "Az3D/Mesh.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <unordered_map>

namespace Az3D {

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
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // Normal attribute (location 1)
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, nrml);

        // Texture coordinate attribute (location 2)
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, txtr);

        return attributeDescriptions;
    }


    bool Mesh::isEmpty() {
        return vertices.empty() || indices.empty();
    }

    // OBJ loader implementation using tiny_obj_loader
    std::shared_ptr<Mesh> Mesh::loadFromOBJ(const char* filePath) {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        // Load the OBJ file
        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath)) {
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
            size_t h1 = std::hash<float>{}(v.pos.x);
            size_t h2 = std::hash<float>{}(v.pos.y);
            size_t h3 = std::hash<float>{}(v.pos.z);
            size_t h4 = std::hash<float>{}(v.txtr.x);
            size_t h5 = std::hash<float>{}(v.txtr.y);
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
                        vertex.pos = {
                            attrib.vertices[3 * index.vertex_index + 0],
                            attrib.vertices[3 * index.vertex_index + 1],
                            attrib.vertices[3 * index.vertex_index + 2]
                        };
                    }

                    // Texture coordinates (optional)
                    if (index.texcoord_index >= 0) {
                        vertex.txtr = {
                            attrib.texcoords[2 * index.texcoord_index + 0],
                            1.0f - attrib.texcoords[2 * index.texcoord_index + 1] // Flip V coordinate
                        };
                    } else {
                        vertex.txtr = {0.0f, 0.0f}; // Default UV
                    }

                    // Normals (optional)
                    if (hasNormals && index.normal_index >= 0) {
                        vertex.nrml = {
                            attrib.normals[3 * index.normal_index + 0],
                            attrib.normals[3 * index.normal_index + 1],
                            attrib.normals[3 * index.normal_index + 2]
                        };
                    }
                }
                
                // Calculate face normal if no normals provided
                if (!hasNormals) {
                    glm::vec3 v0 = triangle[1].pos - triangle[0].pos;
                    glm::vec3 v1 = triangle[2].pos - triangle[0].pos;
                    glm::vec3 faceNormal = glm::normalize(glm::cross(v0, v1));
                    
                    triangle[0].nrml = faceNormal;
                    triangle[1].nrml = faceNormal;
                    triangle[2].nrml = faceNormal;
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




    size_t MeshManager::addMesh(std::shared_ptr<Mesh> mesh) {
        if (!mesh) {
            std::cerr << "Cannot add null mesh" << std::endl;
            return SIZE_MAX; // Invalid index
        }
        
        meshes.push_back(mesh);
        return meshes.size() - 1;
    }

    size_t MeshManager::loadMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
        auto mesh = std::make_shared<Mesh>(std::move(vertices), std::move(indices));
        return addMesh(mesh);
    }
    
    size_t MeshManager::loadMeshFromOBJ(const char* filePath) {
        try {
            auto mesh = Mesh::loadFromOBJ(filePath);
            if (mesh && !mesh->isEmpty()) {
                return addMesh(mesh);
            } else {
                std::cerr << "Failed to load mesh from '" << filePath << "' - mesh is empty or invalid" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to load mesh from '" << filePath << "': " << e.what() << std::endl;
        }
        return SIZE_MAX; // Invalid index
    }

    size_t MeshManager::createQuadMesh() {
        // Create a simple quad mesh (2 triangles)
        std::vector<Vertex> vertices = {
            {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // Bottom-left
            {{ 1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // Bottom-right
            {{ 1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}}, // Top-right
            {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}  // Top-left
        };
        
        std::vector<uint32_t> indices = {
            0, 1, 2,  // First triangle
            2, 3, 0   // Second triangle
        };
        
        auto mesh = std::make_shared<Mesh>(std::move(vertices), std::move(indices));
        return addMesh(mesh);
    }

    size_t MeshManager::createCubeMesh() {
        // Create a simple cube mesh
        std::vector<Vertex> vertices = {
            // Front face
            {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
            {{ 1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            
            // Back face
            {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
            {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
            {{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
            {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
        };
        
        std::vector<uint32_t> indices = {
            // Front face
            0, 1, 2,  2, 3, 0,
            // Back face
            4, 6, 5,  6, 4, 7,
            // Left face
            4, 0, 3,  3, 7, 4,
            // Right face
            1, 5, 6,  6, 2, 1,
            // Top face
            3, 2, 6,  6, 7, 3,
            // Bottom face
            4, 5, 1,  1, 0, 4
        };
        
        auto mesh = std::make_shared<Mesh>(std::move(vertices), std::move(indices));
        return addMesh(mesh);
    }
}
