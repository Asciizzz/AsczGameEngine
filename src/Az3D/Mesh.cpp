#include "Az3D/Mesh.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <unordered_map>

namespace Az3D {

    // Transform implementation - moved from Model.cpp
    void Transform::translate(const glm::vec3& translation) {
        this->pos += translation;
    }

    void Transform::rotate(const glm::quat& quaternion) {
        this->rot = quaternion * this->rot; // Multiply quaternion rots
    }

    void Transform::rotateX(float radians) {
        glm::quat xrot = glm::angleAxis(radians, glm::vec3(1.0f, 0.0f, 0.0f));
        this->rot = xrot * this->rot;
    }

    void Transform::rotateY(float radians) {
        glm::quat yrot = glm::angleAxis(radians, glm::vec3(0.0f, 1.0f, 0.0f));
        this->rot = yrot * this->rot;
    }

    void Transform::rotateZ(float radians) {
        glm::quat zrot = glm::angleAxis(radians, glm::vec3(0.0f, 0.0f, 1.0f));
        this->rot = zrot * this->rot;
    }

    void Transform::scale(float scale) {
        this->scl *= scale;
    }

    void Transform::rotate(const glm::vec3& eulerAngles) {
        // Convert Euler angles to quaternion and apply
        glm::quat eulerQuat = glm::quat(eulerAngles);
        this->rot = eulerQuat * this->rot;
    }

    glm::mat4 Transform::modelMatrix() const {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), pos);
        glm::mat4 rotMat = glm::mat4_cast(rot);
        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), glm::vec3(scl));

        return translation * rotMat * scaleMat;
    }

    void Transform::reset() {
        pos = glm::vec3(0.0f);
        rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        scl = 1.0f;
    }

    // Vertex implementation

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



    size_t MeshManager::addMesh(std::shared_ptr<Mesh> mesh) {
        meshes.push_back(mesh);
        return meshes.size() - 1;
    }
    size_t MeshManager::addMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
        auto mesh = std::make_shared<Mesh>(std::move(vertices), std::move(indices));
        return addMesh(mesh);
    }
    size_t MeshManager::loadFromOBJ(std::string filePath) {
        auto mesh = Mesh::loadFromOBJ(filePath);
        return addMesh(mesh);
    }


    // MESHES AND BOUNDING VOLUMES


    // OBJ loader implementation using tiny_obj_loader
    std::shared_ptr<Mesh> Mesh::loadFromOBJ(std::string filePath) {
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
        std::unordered_map<size_t, uint32_t> uniqueVertices;

        // Hash combine utility
        auto hash_combine = [](std::size_t& seed, std::size_t hash) {
            seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        };

        // Full-attribute hash (position + normal + texcoord)
        auto hashVertex = [&](const Vertex& v) -> size_t {
            size_t seed = 0;
            hash_combine(seed, std::hash<float>{}(v.pos.x));
            hash_combine(seed, std::hash<float>{}(v.pos.y));
            hash_combine(seed, std::hash<float>{}(v.pos.z));
            hash_combine(seed, std::hash<float>{}(v.nrml.x));
            hash_combine(seed, std::hash<float>{}(v.nrml.y));
            hash_combine(seed, std::hash<float>{}(v.nrml.z));
            hash_combine(seed, std::hash<float>{}(v.txtr.x));
            hash_combine(seed, std::hash<float>{}(v.txtr.y));
            return seed;
        };

        bool hasNormals = !attrib.normals.empty();

        for (const auto& shape : shapes) {
            for (size_t f = 0; f < shape.mesh.indices.size(); f += 3) {
                std::array<Vertex, 3> triangle;

                for (int v = 0; v < 3; v++) {
                    const auto& index = shape.mesh.indices[f + v];
                    Vertex& vertex = triangle[v];

                    // Position
                    if (index.vertex_index >= 0) {
                        vertex.pos = {
                            attrib.vertices[3 * index.vertex_index + 0],
                            attrib.vertices[3 * index.vertex_index + 1],
                            attrib.vertices[3 * index.vertex_index + 2]
                        };
                    }

                    // Texture coordinates
                    if (index.texcoord_index >= 0) {
                        vertex.txtr = {
                            attrib.texcoords[2 * index.texcoord_index + 0],
                            1.0f - attrib.texcoords[2 * index.texcoord_index + 1] // Flip V
                        };
                    } else {
                        vertex.txtr = { 0.0f, 0.0f };
                    }

                    // Normals
                    if (hasNormals && index.normal_index >= 0) {
                        vertex.nrml = {
                            attrib.normals[3 * index.normal_index + 0],
                            attrib.normals[3 * index.normal_index + 1],
                            attrib.normals[3 * index.normal_index + 2]
                        };
                    }
                }

                // Generate face normal if needed
                if (!hasNormals) {
                    glm::vec3 edge1 = triangle[1].pos - triangle[0].pos;
                    glm::vec3 edge2 = triangle[2].pos - triangle[0].pos;
                    glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));

                    triangle[0].nrml = faceNormal;
                    triangle[1].nrml = faceNormal;
                    triangle[2].nrml = faceNormal;
                }

                // Deduplicate and add vertices
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
