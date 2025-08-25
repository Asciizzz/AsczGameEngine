#include "Az3D/MeshManager.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "Helpers/tiny_obj_loader.h"


using namespace AzVulk;

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

    void Transform::scale(const glm::vec3& scale) {
        this->scl *= scale;
    }

    glm::mat4 Transform::getMat4() const {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), pos);
        glm::mat4 rotMat = glm::mat4_cast(rot);
        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scl);

        return translation * rotMat * scaleMat;
    }

    void Transform::reset() {
        pos = glm::vec3(0.0f);
        rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        scl = glm::vec3(1.0f);
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



    MeshManager::MeshManager(const AzVulk::Device* vkDevice) : vkDevice(vkDevice) {}

    size_t MeshManager::addMesh(SharedPtr<Mesh> mesh) {
        meshes.push_back(mesh);
        return count++;
    }
    size_t MeshManager::addMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
        auto mesh = MakeShared<Mesh>(std::move(vertices), std::move(indices));
        return addMesh(mesh);
    }
    size_t MeshManager::loadFromOBJ(std::string filePath) {
        auto mesh = Mesh::loadFromOBJ(filePath);
        return addMesh(mesh);
    }

    // Buffer data
    void Mesh::createDeviceBuffer(const Device* vkDevice) {
        BufferData vertexStagingBuffer;
        vertexStagingBuffer.initVkDevice(vkDevice);
        vertexStagingBuffer.setProperties(
            vertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        vertexStagingBuffer.createBuffer();
        vertexStagingBuffer.mappedData(vertices);

        vertexBufferData.initVkDevice(vkDevice);
        vertexBufferData.setProperties(
            vertices.size() * sizeof(Vertex),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        vertexBufferData.createBuffer();

        TemporaryCommand vertexCopyCmd(vkDevice, "Default_Transfer");

        VkBufferCopy vertexCopyRegion{};
        vertexCopyRegion.srcOffset = 0;
        vertexCopyRegion.dstOffset = 0;
        vertexCopyRegion.size = vertices.size() * sizeof(Vertex);

        vkCmdCopyBuffer(vertexCopyCmd.cmdBuffer, vertexStagingBuffer.buffer, vertexBufferData.buffer, 1, &vertexCopyRegion);
        vertexBufferData.hostVisible = false;

        vertexCopyCmd.endAndSubmit();

        
        BufferData indexStagingBuffer;
        indexStagingBuffer.initVkDevice(vkDevice);
        indexStagingBuffer.setProperties(
            indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        indexStagingBuffer.createBuffer();
        indexStagingBuffer.mappedData(indices);

        indexBufferData.initVkDevice(vkDevice);
        indexBufferData.setProperties(
            indices.size() * sizeof(uint32_t),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        indexBufferData.createBuffer();

        TemporaryCommand indexCopyCmd(vkDevice, "Default_Transfer");

        VkBufferCopy indexCopyRegion{};
        indexCopyRegion.srcOffset = 0;
        indexCopyRegion.dstOffset = 0;
        indexCopyRegion.size = indices.size() * sizeof(uint32_t);

        vkCmdCopyBuffer(indexCopyCmd.cmdBuffer, indexStagingBuffer.buffer, indexBufferData.buffer, 1, &indexCopyRegion);
        indexBufferData.hostVisible = false;

        indexCopyCmd.endAndSubmit();
    }

    // OBJ loader implementation using tiny_obj_loader
    SharedPtr<Mesh> Mesh::loadFromOBJ(std::string filePath) {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        // Load the OBJ file
        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath.c_str())) {
            return nullptr;
        }

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        UnorderedMap<size_t, uint32_t> uniqueVertices;

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

        return MakeShared<Mesh>(std::move(vertices), std::move(indices));
    }

    void MeshManager::createBufferDatas() {
        for (size_t i = 0; i < meshes.size(); ++i) {
            meshes[i]->createDeviceBuffer(vkDevice);
        }
    }

}