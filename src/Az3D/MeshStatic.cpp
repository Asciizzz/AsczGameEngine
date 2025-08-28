#include "Az3D/MeshStatic.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "Helpers/tiny_obj_loader.h"


using namespace AzVulk;

namespace Az3D {


MeshStaticGroup::MeshStaticGroup(const AzVulk::Device* vkDevice) : vkDevice(vkDevice) {}

size_t MeshStaticGroup::addMesh(SharedPtr<MeshStatic> mesh) {
    meshes.push_back(mesh);
    return meshes.size() - 1;
}
size_t MeshStaticGroup::addMesh(std::vector<VertexStatic>& vertices, std::vector<uint32_t>& indices) {
    auto mesh = MakeShared<MeshStatic>(std::move(vertices), std::move(indices));
    return addMesh(mesh);
}
size_t MeshStaticGroup::loadFromOBJ(std::string filePath) {
    auto mesh = MeshStatic::loadFromOBJ(filePath);
    return addMesh(mesh);
}

// Buffer data
void MeshStatic::createDeviceBuffer(const Device* vkDevice) {
    BufferData vertexStagingBuffer;
    vertexStagingBuffer.initVkDevice(vkDevice);
    vertexStagingBuffer.setProperties(
        vertices.size() * sizeof(VertexStatic), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    vertexStagingBuffer.createBuffer();
    vertexStagingBuffer.mappedData(vertices.data());

    vertexBufferData.initVkDevice(vkDevice);
    vertexBufferData.setProperties(
        vertices.size() * sizeof(VertexStatic),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    vertexBufferData.createBuffer();

    TemporaryCommand vertexCopyCmd(vkDevice, vkDevice->transferPoolWrapper);

    VkBufferCopy vertexCopyRegion{};
    vertexCopyRegion.srcOffset = 0;
    vertexCopyRegion.dstOffset = 0;
    vertexCopyRegion.size = vertices.size() * sizeof(VertexStatic);

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
    indexStagingBuffer.mappedData(indices.data());

    indexBufferData.initVkDevice(vkDevice);
    indexBufferData.setProperties(
        indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    indexBufferData.createBuffer();

    TemporaryCommand indexCopyCmd(vkDevice, vkDevice->transferPoolWrapper);

    VkBufferCopy indexCopyRegion{};
    indexCopyRegion.srcOffset = 0;
    indexCopyRegion.dstOffset = 0;
    indexCopyRegion.size = indices.size() * sizeof(uint32_t);

    vkCmdCopyBuffer(indexCopyCmd.cmdBuffer, indexStagingBuffer.buffer, indexBufferData.buffer, 1, &indexCopyRegion);
    indexBufferData.hostVisible = false;

    indexCopyCmd.endAndSubmit();
}

// OBJ loader implementation using tiny_obj_loader
SharedPtr<MeshStatic> MeshStatic::loadFromOBJ(std::string filePath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    // Load the OBJ file
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath.c_str())) {
        return nullptr;
    }

    std::vector<VertexStatic> vertices;
    std::vector<uint32_t> indices;
    UnorderedMap<size_t, uint32_t> uniqueVertices;

    // Hash combine utility
    auto hash_combine = [](std::size_t& seed, std::size_t hash) {
        seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };

    // Full-attribute hash (position + normal + texcoord)
    auto hashVertex = [&](const VertexStatic& v) -> size_t {
        size_t seed = 0;
        hash_combine(seed, std::hash<float>{}(v.pos_tu.x));
        hash_combine(seed, std::hash<float>{}(v.pos_tu.y));
        hash_combine(seed, std::hash<float>{}(v.pos_tu.z));
        hash_combine(seed, std::hash<float>{}(v.nrml_tv.x));
        hash_combine(seed, std::hash<float>{}(v.nrml_tv.y));
        hash_combine(seed, std::hash<float>{}(v.nrml_tv.z));
        hash_combine(seed, std::hash<float>{}(v.pos_tu.w));
        hash_combine(seed, std::hash<float>{}(v.nrml_tv.w));
        return seed;
    };

    bool hasNormals = !attrib.normals.empty();

    for (const auto& shape : shapes) {
        for (size_t f = 0; f < shape.mesh.indices.size(); f += 3) {
            std::array<VertexStatic, 3> triangle;

            for (int v = 0; v < 3; v++) {
                const auto& index = shape.mesh.indices[f + v];
                VertexStatic& vertex = triangle[v];

                // Position
                if (index.vertex_index >= 0) {
                    vertex.setPosition({
                        attrib.vertices[3 * index.vertex_index + 0],
                        attrib.vertices[3 * index.vertex_index + 1],
                        attrib.vertices[3 * index.vertex_index + 2]
                    });
                }

                // Texture coordinates
                if (index.texcoord_index >= 0) {
                    vertex.setTextureUV({
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * index.texcoord_index + 1] // Flip V
                    });
                } else {
                    vertex.setTextureUV({ 0.0f, 0.0f });
                }

                // Normals
                if (hasNormals && index.normal_index >= 0) {
                    vertex.setNormal({
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2]
                    });
                }
            }

            // Generate face normal if needed
            if (!hasNormals) {
                glm::vec3 edge1 = triangle[1].getPosition() - triangle[0].getPosition();
                glm::vec3 edge2 = triangle[2].getPosition() - triangle[0].getPosition();
                glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));

                triangle[0].setNormal(faceNormal);
                triangle[1].setNormal(faceNormal);
                triangle[2].setNormal(faceNormal);
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

    return MakeShared<MeshStatic>(std::move(vertices), std::move(indices));
}

void MeshStaticGroup::createDeviceBuffers() {
    for (size_t i = 0; i < meshes.size(); ++i) {
        meshes[i]->createDeviceBuffer(vkDevice);
    }
}

}