#include "Az3D/TinyLoader.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "Helpers/stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION  
#include "Helpers/tiny_obj_loader.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// Prevent TinyGLTF from including its own STB image since we already have it
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE  
#include "Helpers/tiny_gltf.h"

#include <array>
#include <algorithm>
#include <cctype>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Az3D;

void TinyTexture::free() {
    if (data) {
        stbi_image_free(data);
        data = nullptr;
    }
}

TinyTexture TinyLoader::loadImage(const std::string& filePath) {
    TinyTexture texture = {};
    texture.data = stbi_load(
        filePath.c_str(),
        &texture.width,
        &texture.height,
        &texture.channels,
        STBI_rgb_alpha
    );
    
    // Check if loading failed
    if (!texture.data) {
        // Reset dimensions if loading failed
        texture.width = 0;
        texture.height = 0;
        texture.channels = 0;
    }
    
    return texture;
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

// General purpose MeshStatic loader that auto-detects file type
MeshStatic TinyLoader::loadMeshStatic(const std::string& filePath) {
    // Extract file extension
    std::string extension = filePath.substr(filePath.find_last_of(".") + 1);
    
    // Convert to lowercase for case-insensitive comparison
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    if (extension == "obj") {
        return loadMeshStaticFromOBJ(filePath);
    } else if (extension == "gltf" || extension == "glb") {
        return loadMeshStaticFromGLTF(filePath);
    } else {
        throw std::runtime_error("Unsupported MeshStatic file format: " + extension);
    }
}

// OBJ loader implementation using tiny_obj_loader
MeshStatic TinyLoader::loadMeshStaticFromOBJ(const std::string& filePath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    // Load the OBJ file
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath.c_str())) {
        return MeshStatic();
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

    return MeshStatic(std::move(vertices), std::move(indices));
}

MeshStatic TinyLoader::loadMeshStaticFromGLTF(const std::string& filePath) {
    auto meshStatic = MeshStatic();

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;

    // Determine if it's ASCII (.gltf) or binary (.glb) based on file extension
    std::string extension = filePath.substr(filePath.find_last_of(".") + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    bool ok;
    if (extension == "gltf") {
        ok = loader.LoadASCIIFromFile(&model, &err, &warn, filePath);
    } else if (extension == "glb") {
        ok = loader.LoadBinaryFromFile(&model, &err, &warn, filePath);
    } else {
        throw std::runtime_error("Unsupported GLTF file extension: " + extension);
    }
    
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

            meshStatic.vertices.push_back(v);
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
                meshStatic.indices.push_back(index + static_cast<uint32_t>(vertexOffset));
            }
        }

        vertexOffset += vertexCount;
    }

    return meshStatic;
}

TinyRig TinyLoader::loadMeshSkinned(const std::string& filePath, bool loadSkeleton) {
    MeshSkinned meshSkinned;
    RigSkeleton rigSkeleton;

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;

    bool ok; // Check file extension to decide parser
    if (filePath.size() > 4 && filePath.substr(filePath.size() - 4) == ".glb")
        ok = loader.LoadBinaryFromFile(&model, &err, &warn, filePath); // GLB
    else
        ok = loader.LoadASCIIFromFile(&model, &err, &warn, filePath);  // GLTF

    if (!ok) throw std::runtime_error("GLTF load error: " + err);

    if (model.meshes.empty()) throw std::runtime_error("GLTF has no meshes: " + filePath);

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
            VertexSkinned v{};
            v.pos_tu    = glm::vec4(positions.size() > i ? positions[i] : glm::vec3(0.0f),
                                    uvs.size() > i ? uvs[i].x : 0.0f);
            v.nrml_tv   = glm::vec4(normals.size() > i ? normals[i] : glm::vec3(0.0f),
                                    uvs.size() > i ? uvs[i].y : 0.0f);
            v.tangent   = tangents.size() > i ? tangents[i] : glm::vec4(1,0,0,1);
            v.boneIDs   = joints.size() > i ? joints[i] : glm::uvec4(0);
            v.weights   = weights.size() > i ? weights[i] : glm::vec4(0,0,0,0);

            meshSkinned.vertices.push_back(v);
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
                meshSkinned.indices.push_back(index + static_cast<uint32_t>(vertexOffset));
            }
        }

        vertexOffset += vertexCount;
    }

    // Skeleton (take first skin if present, only if loadSkeleton is true)
    if (loadSkeleton && !model.skins.empty()) {
        const tinygltf::Skin& skin = model.skins[0];

        // Inverse bind matrices
        std::vector<glm::mat4> ibms;
        if (skin.inverseBindMatrices >= 0) {
            readAccessor(model, model.accessors[skin.inverseBindMatrices], ibms);
        }

        rigSkeleton.names.reserve(skin.joints.size());
        rigSkeleton.parentIndices.reserve(skin.joints.size());
        rigSkeleton.inverseBindMatrices.reserve(skin.joints.size());
        rigSkeleton.localBindTransforms.reserve(skin.joints.size());

        for (size_t i = 0; i < skin.joints.size(); i++) {
            int nodeIndex = skin.joints[i];
            const auto& node = model.nodes[nodeIndex];

            std::string boneName = node.name.empty() ? ("bone_" + std::to_string(i)) : node.name;

            // Fill parent index: if node has a parent that's also a joint, link it
            int boneParentIndex = -1;
            for (size_t p = 0; p < model.nodes.size(); p++) {
                const auto& parent = model.nodes[p];
                for (int childIdx : parent.children) {
                    if (childIdx == nodeIndex) {
                        // Check if parent is part of this skin
                        auto it = std::find(skin.joints.begin(), skin.joints.end(), p);
                        if (it != skin.joints.end()) {
                            boneParentIndex = static_cast<int>(std::distance(skin.joints.begin(), it));
                        }
                    }
                }
            }

            glm::mat4 boneInverseBindMatrix = (ibms.size() > i) ? ibms[i] : glm::mat4(1.0f);

            glm::mat4 boneLocalBindTransform;
            if (node.matrix.size() == 16) {
                boneLocalBindTransform = glm::make_mat4(node.matrix.data());
            } else {
                glm::mat4 t = glm::translate(glm::mat4(1.0f),
                                            node.translation.size() ? glm::vec3(node.translation[0], node.translation[1], node.translation[2]) : glm::vec3(0));
                glm::mat4 r = glm::mat4(1.0f);
                if (node.rotation.size() == 4) {
                    glm::quat q(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
                    r = glm::mat4_cast(q);
                }
                glm::mat4 s = glm::scale(glm::mat4(1.0f),
                                        node.scale.size() ? glm::vec3(node.scale[0], node.scale[1], node.scale[2]) : glm::vec3(1));
                boneLocalBindTransform = t * r * s;
            }

            rigSkeleton.nameToIndex[boneName] = static_cast<int>(rigSkeleton.names.size());

            rigSkeleton.names.push_back(boneName);
            rigSkeleton.parentIndices.push_back(boneParentIndex);
            rigSkeleton.inverseBindMatrices.push_back(boneInverseBindMatrix);
            rigSkeleton.localBindTransforms.push_back(boneLocalBindTransform);
        }
    } else if (loadSkeleton) {
        // Create 1 default bone if no skeleton found but loadSkeleton is true
        rigSkeleton.names.push_back("sad_bone");
        rigSkeleton.parentIndices.push_back(-1);
        rigSkeleton.inverseBindMatrices.push_back(glm::mat4(1.0f));
        rigSkeleton.localBindTransforms.push_back(glm::mat4(1.0f));
    }

    // Return TinyRig with mesh and skeleton
    TinyRig rig;
    rig.mesh = meshSkinned;
    rig.skeleton = loadSkeleton ? rigSkeleton : RigSkeleton();
    return rig;
}