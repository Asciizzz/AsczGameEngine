#include "Az3D/MeshStatic.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "Helpers/tiny_obj_loader.h"
#include "Helpers/tiny_gltf.h"

#include <array>


using namespace AzVulk;
using namespace Az3D;

// OBJ loader implementation using tiny_obj_loader
SharedPtr<MeshStatic> MeshStatic::loadFromOBJ(const std::string& filePath) {
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
            std::vector<VertexStatic> triangle(3);

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

// Utility: read GLTF accessor as typed array
template<typename T>
void readAccessor(const tinygltf::Model& model, const tinygltf::Accessor& accessor, std::vector<T>& out) {
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buf = model.buffers[view.buffer];

    const unsigned char* dataPtr = buf.data.data() + view.byteOffset + accessor.byteOffset;
    size_t stride = accessor.ByteStride(view);
    out.resize(accessor.count);

    for (size_t i = 0; i < accessor.count; i++) {
        const void* ptr = dataPtr + stride * i;
        memcpy(&out[i], ptr, sizeof(T));
    }
}

SharedPtr<MeshStatic> MeshStatic::loadFromGLTF(const std::string& filePath) {
    auto meshStatic = std::make_shared<MeshStatic>();

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;

    bool ok = loader.LoadASCIIFromFile(&model, &err, &warn, filePath);
    if (!ok) {
        throw std::runtime_error("GLTF load error: " + err);
    }

    if (model.meshes.empty()) {
        throw std::runtime_error("GLTF has no meshes: " + filePath);
    }

    const tinygltf::Mesh& mesh = model.meshes[0];
    size_t vertexOffset = 0;

    // Merge all primitives into one vertex/index buffer
    for (const auto& primitive : mesh.primitives) {
        size_t vertexCount = model.accessors[primitive.attributes.at("POSITION")].count;
        std::vector<glm::vec3> positions, normals;
        std::vector<glm::vec4> tangents, weights;
        std::vector<glm::vec2> uvs;
        std::vector<glm::uvec4> joints;

        // Read attributes (skip missing gracefully)
        if (primitive.attributes.count("POSITION"))
            readAccessor(model, model.accessors[primitive.attributes.at("POSITION")], positions);
        if (primitive.attributes.count("NORMAL"))
            readAccessor(model, model.accessors[primitive.attributes.at("NORMAL")], normals);
        if (primitive.attributes.count("TANGENT"))
            readAccessor(model, model.accessors[primitive.attributes.at("TANGENT")], tangents);
        if (primitive.attributes.count("TEXCOORD_0"))
            readAccessor(model, model.accessors[primitive.attributes.at("TEXCOORD_0")], uvs);
        if (primitive.attributes.count("JOINTS_0"))
            readAccessor(model, model.accessors[primitive.attributes.at("JOINTS_0")], joints);
        if (primitive.attributes.count("WEIGHTS_0"))
            readAccessor(model, model.accessors[primitive.attributes.at("WEIGHTS_0")], weights);

        // Expand into vertices
        for (size_t i = 0; i < vertexCount; i++) {
            VertexStatic v{};
            v.pos_tu     = glm::vec4(positions.size() > i ? positions[i] : glm::vec3(0.0f),
                                     uvs.size() > i ? uvs[i].x : 0.0f);
            v.nrml_tv    = glm::vec4(normals.size() > i ? normals[i] : glm::vec3(0.0f),
                                     uvs.size() > i ? uvs[i].y : 0.0f);
            v.tangent    = tangents.size() > i ? tangents[i] : glm::vec4(1,0,0,1);

            meshStatic->vertices.push_back(v);
        }

        // Indices
        if (primitive.indices >= 0) {
            const auto& indexAccessor = model.accessors[primitive.indices];
            const auto& indexBufferView = model.bufferViews[indexAccessor.bufferView];
            const auto& indexBuffer = model.buffers[indexBufferView.buffer];
            const unsigned char* dataPtr = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;
            size_t stride = indexAccessor.ByteStride(indexBufferView);

            for (size_t i = 0; i < indexAccessor.count; i++) {
                uint32_t index = 0;
                switch (indexAccessor.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        index = *((uint8_t*)(dataPtr + stride * i));
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        index = *((uint16_t*)(dataPtr + stride * i));
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        index = *((uint32_t*)(dataPtr + stride * i));
                        break;
                    default:
                        throw std::runtime_error("Unsupported index component type");
                }
                meshStatic->indices.push_back(index + static_cast<uint32_t>(vertexOffset));
            }
        }

        vertexOffset += vertexCount;
    }

    return meshStatic;
}
