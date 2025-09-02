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
#include <cstring>
#include <unordered_map>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Az3D;

// Custom image loading callback for tinygltf since we disabled STB_IMAGE
bool LoadImageData(tinygltf::Image* image, const int image_idx, std::string* err,
                   std::string* warn, int req_width, int req_height,
                   const unsigned char* bytes, int size, void* user_data) {
    (void)image_idx;
    (void)req_width;
    (void)req_height;
    (void)user_data;

    int width, height, channels;
    unsigned char* data = stbi_load_from_memory(bytes, size, &width, &height, &channels, 0);
    
    if (!data) {
        if (err) {
            *err = "Failed to load image data from memory";
        }
        return false;
    }

    image->width = width;
    image->height = height;
    image->component = channels;
    image->bits = 8;  // stbi always loads 8-bit per channel
    image->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
    image->image.resize(width * height * channels);
    std::memcpy(&image->image[0], data, width * height * channels);
    
    stbi_image_free(data);
    return true;
}

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

// General purpose StaticMesh loader that auto-detects file type
StaticMesh TinyLoader::loadStaticMesh(const std::string& filePath) {
    // Extract file extension
    std::string extension = filePath.substr(filePath.find_last_of(".") + 1);
    
    // Convert to lowercase for case-insensitive comparison
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    if (extension == "obj") {
        return loadStaticMeshFromOBJ(filePath);
    } else if (extension == "gltf" || extension == "glb") {
        return loadStaticMeshFromGLTF(filePath);
    } else {
        throw std::runtime_error("Unsupported StaticMesh file format: " + extension);
    }
}

// OBJ loader implementation using tiny_obj_loader
StaticMesh TinyLoader::loadStaticMeshFromOBJ(const std::string& filePath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    // Load the OBJ file
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath.c_str())) {
        return StaticMesh();
    }

    std::vector<StaticVertex> vertices;
    std::vector<uint32_t> indices;
    UnorderedMap<size_t, uint32_t> uniqueVertices;

    // Hash combine utility
    auto hash_combine = [](std::size_t& seed, std::size_t hash) {
        seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };

    // Full-attribute hash (position + normal + texcoord)
    auto hashVertex = [&](const StaticVertex& v) -> size_t {
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
            std::vector<StaticVertex> triangle(3);

            for (int v = 0; v < 3; v++) {
                const auto& index = shape.mesh.indices[f + v];
                StaticVertex& vertex = triangle[v];

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

    return StaticMesh(std::move(vertices), std::move(indices));
}

StaticMesh TinyLoader::loadStaticMeshFromGLTF(const std::string& filePath) {
    auto staticMesh = StaticMesh();

    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(LoadImageData, nullptr);
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
            StaticVertex v{};
            v.pos_tu     = glm::vec4(positions.size() > i ? positions[i] : glm::vec3(0.0f),
                                     uvs.size() > i ? uvs[i].x : 0.0f);
            v.nrml_tv    = glm::vec4(normals.size() > i ? normals[i] : glm::vec3(0.0f),
                                     uvs.size() > i ? uvs[i].y : 0.0f);
            v.tangent    = tangents.size() > i ? tangents[i] : glm::vec4(1,0,0,1);

            staticMesh.vertices.push_back(v);
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
                staticMesh.indices.push_back(index + static_cast<uint32_t>(vertexOffset));
            }
        }

        vertexOffset += vertexCount;
    }

    return staticMesh;
}







// Utility: make local transform from a glTF node
static glm::mat4 makeLocalFromNode(const tinygltf::Node& node) {
    if (node.matrix.size() == 16) {
        // Read as double then cast to float (column-major)
        glm::dmat4 dm = glm::make_mat4x4<double>(node.matrix.data());
        return glm::mat4(dm);
    } else {
        // Default TRS
        glm::dvec3 t(0.0);
        if (node.translation.size() == 3)
            t = { node.translation[0], node.translation[1], node.translation[2] };

        glm::dquat q(1.0, 0.0, 0.0, 0.0); // w,x,y,z
        if (node.rotation.size() == 4)
            q = { node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2] };

        glm::dvec3 s(1.0);
        if (node.scale.size() == 3) {
            // Guard against bad export zero-scale
            double sx = (node.scale[0] == 0.0) ? 1.0 : node.scale[0];
            double sy = (node.scale[1] == 0.0) ? 1.0 : node.scale[1];
            double sz = (node.scale[2] == 0.0) ? 1.0 : node.scale[2];
            s = { sx, sy, sz };
        }

        glm::dmat4 M = glm::translate(glm::dmat4(1.0), t)
                     * glm::mat4_cast(q)
                     * glm::scale(glm::dmat4(1.0), s);
        return glm::mat4(M);
    }
}

TinyModel TinyLoader::loadRigMesh(const std::string& filePath, bool loadRig) {
    RigMesh rigMesh;
    RigSkeleton rigSkeleton;

    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(LoadImageData, nullptr);
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
            RigVertex v{};
            v.pos_tu    = glm::vec4(positions.size() > i ? positions[i] : glm::vec3(0.0f),
                                    uvs.size() > i ? uvs[i].x : 0.0f);
            v.nrml_tv   = glm::vec4(normals.size() > i ? normals[i] : glm::vec3(0.0f),
                                    uvs.size() > i ? uvs[i].y : 0.0f);
            v.tangent   = tangents.size() > i ? tangents[i] : glm::vec4(1,0,0,1);
            v.boneIDs   = joints.size() > i ? joints[i] : glm::uvec4(0);
            v.weights   = weights.size() > i ? weights[i] : glm::vec4(1.0f,0,0,0);

            rigMesh.vertices.push_back(v);
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
                rigMesh.indices.push_back(index + static_cast<uint32_t>(vertexOffset));
            }
        }

        vertexOffset += vertexCount;
    }

    // Skeleton (take first skin if present, only if loadRig is true)
    if (loadRig && !model.skins.empty()) {
        const tinygltf::Skin& skin = model.skins[0];

        // Create mapping from global node index to local bone index
        std::unordered_map<int, int> nodeIndexToBoneIndex;
        for (size_t i = 0; i < skin.joints.size(); i++) {
            nodeIndexToBoneIndex[skin.joints[i]] = static_cast<int>(i);
        }

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

            // Find parent index (only if parent is also a joint)
            int boneParentIndex = -1;
            for (size_t p = 0; p < model.nodes.size(); p++) {
                const auto& parent = model.nodes[p];
                for (int childIdx : parent.children) {
                    if (childIdx == nodeIndex) {
                        auto it = std::find(skin.joints.begin(), skin.joints.end(), p);
                        if (it != skin.joints.end()) {
                            boneParentIndex = static_cast<int>(std::distance(skin.joints.begin(), it));
                        }
                    }
                }
            }

            glm::mat4 boneInverseBindMatrix = (ibms.size() > i) ? ibms[i] : glm::mat4(1.0f);

            glm::mat4 boneLocalBindTransform = makeLocalFromNode(node);

            rigSkeleton.nameToIndex[boneName] = static_cast<int>(rigSkeleton.names.size());
            rigSkeleton.names.push_back(boneName);
            rigSkeleton.parentIndices.push_back(boneParentIndex);
            rigSkeleton.inverseBindMatrices.push_back(boneInverseBindMatrix);
            rigSkeleton.localBindTransforms.push_back(boneLocalBindTransform);
        }

        // Now remap bone IDs in vertices from global node indices to local bone indices
        for (auto& vertex : rigMesh.vertices) {
            for (int j = 0; j < 4; j++) {
                int globalBoneId = vertex.boneIDs[j];
                auto it = nodeIndexToBoneIndex.find(globalBoneId);
                if (it != nodeIndexToBoneIndex.end()) {
                    vertex.boneIDs[j] = it->second;  // Remap to local bone index
                } else {
                    // If bone ID not found in skeleton, set to 0 (fallback)
                    vertex.boneIDs[j] = 0;
                }
            }
        }
    } else if (loadRig) {
        // Fallback skeleton with 1 bone
        rigSkeleton.names.push_back("sad_bone");
        rigSkeleton.parentIndices.push_back(-1);
        rigSkeleton.inverseBindMatrices.push_back(glm::mat4(1.0f));
        rigSkeleton.localBindTransforms.push_back(glm::mat4(1.0f));
    }

    // Return TinyModel with mesh and skeleton
    TinyModel rig;
    rig.mesh = rigMesh;
    rig.rig = loadRig ? rigSkeleton : RigSkeleton();
    return rig;
}

/*
// Debug:

ID[122] - w[1], ID[122] - w[0], ID[31612] - w[0], ID[31612] - w[0],
ID[122] - w[1], ID[31612] - w[0], ID[31612] - w[0], ID[31612] - w[0],
ID[31612] - w[0.571477], ID[31612] - w[0.428523], ID[31612] - w[0], ID[31612] - w[0],
ID[31612] - w[0.571477], ID[31612] - w[0.428523], ID[31612] - w[0], ID[31612] - w[0],
ID[31612] - w[0.570998], ID[31612] - w[0.429002], ID[31612] - w[0], ID[31612] - w[0],
ID[31612] - w[0.570998], ID[31612] - w[0.429002], ID[31612] - w[0], ID[30584] - w[0],
ID[31612] - w[0.516174], ID[31612] - w[0.483826], ID[30584] - w[0], ID[30584] - w[0],
ID[31612] - w[0.516174], ID[30584] - w[0.483826], ID[30584] - w[0], ID[30208] - w[0],
ID[30584] - w[0.800001], ID[30584] - w[0.199999], ID[30208] - w[0], ID[30584] - w[0],
ID[30584] - w[0.800001], ID[30208] - w[0.199999], ID[30584] - w[0], ID[30584] - w[0],

DANGER DANGER!!! WHY IS THERE BONE ID THE SIZE OF 1500 PEOPLE WHILE THE MAX BONE COUNT IS JUST 126 IN MY MODEL


*/