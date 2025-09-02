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

// Utility: read GLTF accessor as typed array with proper error checking
template<typename T>
bool readAccessorSafe(const tinygltf::Model& model, int accessorIndex, std::vector<T>& out) {
    if (accessorIndex < 0 || accessorIndex >= static_cast<int>(model.accessors.size())) {
        return false;
    }
    
    const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
    
    if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size())) {
        return false;
    }
    
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    
    if (view.buffer < 0 || view.buffer >= static_cast<int>(model.buffers.size())) {
        return false;
    }
    
    const tinygltf::Buffer& buf = model.buffers[view.buffer];

    const unsigned char* dataPtr = buf.data.data() + view.byteOffset + accessor.byteOffset;
    size_t stride = accessor.ByteStride(view);
    
    if (stride == 0) {
        stride = sizeof(T);
    }
    
    out.resize(accessor.count);

    for (size_t i = 0; i < accessor.count; i++) {
        const void* ptr = dataPtr + stride * i;
        memcpy(&out[i], ptr, sizeof(T));
    }
    
    return true;
}

// Utility: read joint indices with proper component type handling
bool readJointIndices(const tinygltf::Model& model, int accessorIndex, std::vector<glm::uvec4>& joints) {
    if (accessorIndex < 0 || accessorIndex >= static_cast<int>(model.accessors.size())) {
        return false;
    }
    
    const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buf = model.buffers[view.buffer];
    
    const unsigned char* dataPtr = buf.data.data() + view.byteOffset + accessor.byteOffset;
    size_t stride = accessor.ByteStride(view);
    
    // Calculate stride based on component type if not specified
    if (stride == 0) {
        switch (accessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                stride = 4; // VEC4 of bytes
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                stride = 8; // VEC4 of shorts
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                stride = 16; // VEC4 of ints
                break;
            default:
                std::cout << "ERROR: Unsupported joint component type: " << accessor.componentType << std::endl;
                return false;
        }
    }
    
    joints.resize(accessor.count);
    
    for (size_t i = 0; i < accessor.count; i++) {
        const unsigned char* vertexDataPtr = dataPtr + stride * i;
        glm::uvec4 jointIndices(0);
        
        switch (accessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                const uint8_t* indices = reinterpret_cast<const uint8_t*>(vertexDataPtr);
                jointIndices = glm::uvec4(indices[0], indices[1], indices[2], indices[3]);
                break;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                const uint16_t* indices = reinterpret_cast<const uint16_t*>(vertexDataPtr);
                jointIndices = glm::uvec4(indices[0], indices[1], indices[2], indices[3]);
                break;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                const uint32_t* indices = reinterpret_cast<const uint32_t*>(vertexDataPtr);
                jointIndices = glm::uvec4(indices[0], indices[1], indices[2], indices[3]);
                break;
            }
            default:
                std::cout << "ERROR: Unsupported joint component type in processing: " << accessor.componentType << std::endl;
                return false;
        }
        
        joints[i] = jointIndices;
    }
    
    return true;
}

// Basic readAccessor for backward compatibility
template<typename T>
void readAccessor(const tinygltf::Model& model, const tinygltf::Accessor& accessor, std::vector<T>& out) {
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];
    
    const unsigned char* dataPtr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
    size_t stride = accessor.ByteStride(bufferView);
    
    if (stride == 0) {
        stride = sizeof(T);
    }
    
    out.resize(accessor.count);
    
    for (size_t i = 0; i < accessor.count; i++) {
        std::memcpy(&out[i], dataPtr + stride * i, sizeof(T));
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
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    loader.SetImageLoader(LoadImageData, nullptr);

    RigMesh rigMesh;
    RigSkeleton rigSkeleton;

    bool ok;
    if (filePath.find(".glb") != std::string::npos) {
        ok = loader.LoadBinaryFromFile(&model, &err, &warn, filePath);  // GLB
    } else {
        ok = loader.LoadASCIIFromFile(&model, &err, &warn, filePath);  // GLTF
    }

    if (!ok) {
        throw std::runtime_error("GLTF load error: " + err);
    }

    if (model.meshes.empty()) {
        throw std::runtime_error("GLTF has no meshes: " + filePath);
    }

    // Build skeleton first if we need it
    std::unordered_map<int, int> nodeIndexToBoneIndex;
    
    if (loadRig && !model.skins.empty()) {
        const tinygltf::Skin& skin = model.skins[0];
        
        std::cout << "=== BUILDING SKELETON ===\n";
        std::cout << "Found skin with " << skin.joints.size() << " joints\n";
        
        // Create the node-to-bone mapping
        for (size_t i = 0; i < skin.joints.size(); i++) {
            nodeIndexToBoneIndex[skin.joints[i]] = static_cast<int>(i);
        }
        
        // Load inverse bind matrices
        std::vector<glm::mat4> inverseBindMatrices;
        if (skin.inverseBindMatrices >= 0) {
            if (!readAccessorSafe(model, skin.inverseBindMatrices, inverseBindMatrices)) {
                throw std::runtime_error("Failed to read inverse bind matrices");
            }
            std::cout << "Loaded " << inverseBindMatrices.size() << " inverse bind matrices\n";
        } else {
            std::cout << "No inverse bind matrices - using identity matrices\n";
            inverseBindMatrices.resize(skin.joints.size(), glm::mat4(1.0f));
        }
        
        // Build skeleton structure
        rigSkeleton.names.reserve(skin.joints.size());
        rigSkeleton.parentIndices.reserve(skin.joints.size());
        rigSkeleton.inverseBindMatrices.reserve(skin.joints.size());
        rigSkeleton.localBindTransforms.reserve(skin.joints.size());
        
        // First pass: gather bone data
        for (size_t i = 0; i < skin.joints.size(); i++) {
            int nodeIndex = skin.joints[i];
            
            if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size())) {
                throw std::runtime_error("Invalid joint node index: " + std::to_string(nodeIndex));
            }
            
            const auto& node = model.nodes[nodeIndex];
            std::string boneName = node.name.empty() ? ("bone_" + std::to_string(i)) : node.name;
            
            rigSkeleton.names.push_back(boneName);
            rigSkeleton.parentIndices.push_back(-1); // Will be fixed in second pass
            rigSkeleton.inverseBindMatrices.push_back(inverseBindMatrices.size() > i ? inverseBindMatrices[i] : glm::mat4(1.0f));
            rigSkeleton.localBindTransforms.push_back(makeLocalFromNode(node));
            rigSkeleton.nameToIndex[boneName] = static_cast<int>(i);
        }
        
        // Second pass: fix parent relationships
        for (size_t i = 0; i < skin.joints.size(); i++) {
            int nodeIndex = skin.joints[i];
            int parentBoneIndex = -1;
            
            // Find which node is the parent of this node
            for (size_t nodeIdx = 0; nodeIdx < model.nodes.size(); nodeIdx++) {
                const auto& candidateParent = model.nodes[nodeIdx];
                
                // Check if this node is a child of candidateParent
                for (int childIdx : candidateParent.children) {
                    if (childIdx == nodeIndex) {
                        // Found parent, check if it's also in the skeleton
                        auto it = nodeIndexToBoneIndex.find(static_cast<int>(nodeIdx));
                        if (it != nodeIndexToBoneIndex.end()) {
                            parentBoneIndex = it->second;
                        }
                        break;
                    }
                }
                
                if (parentBoneIndex != -1) break;
            }
            
            rigSkeleton.parentIndices[i] = parentBoneIndex;
            
            std::cout << "Bone " << i << " (" << rigSkeleton.names[i] << ") parent: ";
            if (parentBoneIndex == -1) {
                std::cout << "ROOT\n";
            } else {
                std::cout << parentBoneIndex << " (" << rigSkeleton.names[parentBoneIndex] << ")\n";
            }
        }
        
        std::cout << "=== SKELETON COMPLETE ===\n\n";
    }

    const tinygltf::Mesh& mesh = model.meshes[0];
    size_t vertexOffset = 0;

    // Mesh processing with rigorous validation
    for (const auto& primitive : mesh.primitives) {
        size_t vertexCount = model.accessors[primitive.attributes.at("POSITION")].count;
        
        std::cout << "=== PROCESSING PRIMITIVE ===\n";
        std::cout << "Vertex count: " << vertexCount << "\n";

        std::vector<glm::vec3> positions, normals;
        std::vector<glm::vec4> tangents, weights;
        std::vector<glm::vec2> uvs;
        std::vector<glm::uvec4> joints;

        // Read standard attributes
        if (primitive.attributes.count("POSITION")) {
            readAccessor(model, model.accessors[primitive.attributes.at("POSITION")], positions);
        }
        if (primitive.attributes.count("NORMAL")) {
            readAccessor(model, model.accessors[primitive.attributes.at("NORMAL")], normals);
        }
        if (primitive.attributes.count("TANGENT")) {
            readAccessor(model, model.accessors[primitive.attributes.at("TANGENT")], tangents);
        }
        if (primitive.attributes.count("TEXCOORD_0")) {
            readAccessor(model, model.accessors[primitive.attributes.at("TEXCOORD_0")], uvs);
        }
        
        // Read skinning data with robust error handling
        if (loadRig && primitive.attributes.count("JOINTS_0") && primitive.attributes.count("WEIGHTS_0")) {
            std::cout << "Reading skinning data...\n";
            
            if (!readJointIndices(model, primitive.attributes.at("JOINTS_0"), joints)) {
                throw std::runtime_error("Failed to read joint indices");
            }
            
            if (!readAccessorSafe(model, primitive.attributes.at("WEIGHTS_0"), weights)) {
                throw std::runtime_error("Failed to read bone weights");
            }
            
            std::cout << "Read " << joints.size() << " joint sets and " << weights.size() << " weight sets\n";
        }

        // Build vertices with thorough validation
        int problemVertices = 0;
        int rootDependentVertices = 0;
        
        for (size_t i = 0; i < vertexCount; i++) {
            RigVertex vertex{};
            
            vertex.pos_tu = glm::vec4(
                positions.size() > i ? positions[i] : glm::vec3(0.0f),
                uvs.size() > i ? uvs[i].x : 0.0f
            );
            vertex.nrml_tv = glm::vec4(
                normals.size() > i ? normals[i] : glm::vec3(0.0f),
                uvs.size() > i ? uvs[i].y : 0.0f
            );
            vertex.tangent = tangents.size() > i ? tangents[i] : glm::vec4(1,0,0,1);
            
            if (loadRig && joints.size() > i && weights.size() > i) {
                glm::uvec4 jointIds = joints[i];
                glm::vec4 boneWeights = weights[i];
                
                // Validate joint indices are within skeleton range
                bool hasInvalidJoint = false;
                for (int j = 0; j < 4; j++) {
                    if (boneWeights[j] > 0.0f && jointIds[j] >= rigSkeleton.names.size()) {
                        hasInvalidJoint = true;
                        problemVertices++;
                        break;
                    }
                }
                
                if (hasInvalidJoint) {
                    // Set to rest pose (root bone only)
                    vertex.boneIDs = glm::ivec4(0, 0, 0, 0);
                    vertex.weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                } else {
                    // Normalize weights to ensure they sum to 1.0
                    float weightSum = boneWeights.x + boneWeights.y + boneWeights.z + boneWeights.w;
                    if (weightSum > 0.0f) {
                        boneWeights /= weightSum;
                    } else {
                        boneWeights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                        jointIds = glm::uvec4(0, 0, 0, 0);
                    }
                    
                    vertex.boneIDs = glm::ivec4(jointIds);
                    vertex.weights = boneWeights;
                    
                    // Check for root dependency
                    if (boneWeights.x > 0.99f && jointIds.x == 0) {
                        rootDependentVertices++;
                    }
                }
                
                // Debug sample output
                if (i < 5) {
                    std::cout << "Vertex " << i << ": ";
                    for (int j = 0; j < 4; j++) {
                        if (vertex.weights[j] > 0.0f) {
                            std::cout << "Bone[" << vertex.boneIDs[j] << "]:Weight[" << vertex.weights[j] << "] ";
                        }
                    }
                    std::cout << "\n";
                }
            } else {
                // No rigging - bind to root
                vertex.boneIDs = glm::ivec4(0, 0, 0, 0);
                vertex.weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
            }
            
            rigMesh.vertices.push_back(vertex);
        }
        
        std::cout << "Problem vertices fixed: " << problemVertices << "\n";
        std::cout << "Root-dependent vertices: " << rootDependentVertices << "\n";

        // Handle indices
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

    std::cout << "=== MESH COMPLETE ===\n";
    std::cout << "Total vertices: " << rigMesh.vertices.size() << "\n";
    std::cout << "Total indices: " << rigMesh.indices.size() << "\n\n";

    return TinyModel{rigMesh, rigSkeleton};
}