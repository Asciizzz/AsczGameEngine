#include "TinyEngine/TinyLoader.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "Helpers/stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION  
#include "Helpers/tiny_obj_loader.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE  
#include "Helpers/tiny_gltf.h"

#include <array>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

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

TinyTexture TinyLoader::loadTexture(const std::string& filePath) {
    TinyTexture texture = {};

    // Load image using stbi with original channel count
    uint8_t* stbiData = stbi_load(
        filePath.c_str(),
        &texture.width,
        &texture.height,
        &texture.channels,
        0  // Don't force channel conversion, preserve original format
    );
    
    // Check if loading failed
    if (!stbiData) {
        // Reset dimensions if loading failed
        texture.width = 0;
        texture.height = 0;
        texture.channels = 0;
        texture.data.clear();
        texture.makeHash();
        return texture;
    }

    size_t dataSize = texture.width * texture.height * texture.channels;
    texture.data.resize(dataSize);
    std::memcpy(texture.data.data(), stbiData, dataSize);
    // Free stbi allocated memory
    stbi_image_free(stbiData);

    texture.makeHash();
    return texture;
}

// =================================== 3D MODELS ===================================


// Utility: read GLTF accessor as typed array with proper error checking
template<typename T>
bool readAccessor(const tinygltf::Model& model, int accessorIndex, std::vector<T>& out) {
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

template<typename T>
bool readAccessorFromMap(const tinygltf::Model& model, const std::map<std::string, int>& attributes, const std::string& key, std::vector<T>& out) {
    if (attributes.count(key) == 0) return false;

    return readAccessor(model, attributes.at(key), out);
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
                return false;
        }
        
        joints[i] = jointIndices;
    }
    
    return true;
}


// ============================================================================
// ===================== TinyLoader Implementation ===========================
// ============================================================================

std::string TinyLoader::sanitizeAsciiz(const std::string& originalName, const std::string& key, size_t fallbackIndex) {
    if (originalName.empty()) {
        return key + "_" + std::to_string(fallbackIndex);
    }
    
    // Check if name is already ASCII-safe (letters, numbers, underscore)
    bool isAsciiSafe = true;
    for (char c : originalName) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            isAsciiSafe = false;
            break;
        }
    }
    
    if (isAsciiSafe && !originalName.empty() && !std::isdigit(originalName[0])) {
        // Name is already safe and doesn't start with a digit
        return originalName;
    }
    
    // Generate hash from original name for uniqueness
    std::hash<std::string> hasher;
    size_t nameHash = hasher(originalName);
    
    // Create ASCII-safe identifier
    std::string safeName = key + "_";
    
    // Try to preserve recognizable ASCII parts
    for (char c : originalName) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            safeName += c;
            if (safeName.length() > 30) break; // Limit length (increased for longer keys)
        }
    }
    
    // If no ASCII chars were preserved, use the fallback index
    if (safeName == key + "_") {
        safeName += std::to_string(fallbackIndex);
    }
    
    // Add hash suffix for uniqueness (truncated to 4 hex digits)
    char hashHex[8];
    snprintf(hashHex, sizeof(hashHex), "%04X", static_cast<unsigned int>(nameHash & 0xFFFF));
    safeName += "_0x" + std::string(hashHex);
    
    // Ensure it doesn't start with a digit
    if (!safeName.empty() && std::isdigit(safeName[0])) {
        safeName = key + "_" + safeName;
    }
    
    return safeName;
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

TinyModel TinyLoader::loadModelFromGLTF(const std::string& filePath, bool forceStatic) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    loader.SetImageLoader(LoadImageData, nullptr);
    loader.SetPreserveImageChannels(true);  // Preserve original channel count

    TinyModel result;

    bool ok;
    if (filePath.find(".glb") != std::string::npos) {
        ok = loader.LoadBinaryFromFile(&model, &err, &warn, filePath);  // GLB
    } else {
        ok = loader.LoadASCIIFromFile(&model, &err, &warn, filePath);  // GLTF
    }

    if (!ok || model.meshes.empty()) return TinyModel();

    result.textures.reserve(model.textures.size());
    for (const auto& gltfTexture : model.textures) {
        TinyTexture texture;

        // Load image data
        if (gltfTexture.source >= 0 && gltfTexture.source < static_cast<int>(model.images.size())) {
            const auto& image = model.images[gltfTexture.source];
            texture.
                setDimensions(image.width, image.height).
                setChannels(image.component).
                setData(image.image).
                makeHash();
        }
        
        // Load sampler settings (address mode)
        texture.addressMode = TinyTexture::AddressMode::Repeat; // Default
        if (gltfTexture.sampler >= 0 && gltfTexture.sampler < static_cast<int>(model.samplers.size())) {
            const auto& sampler = model.samplers[gltfTexture.sampler];
            
            // Convert GLTF wrap modes to our AddressMode enum
            // GLTF uses the same values for both wrapS and wrapT, so we'll use wrapS
            switch (sampler.wrapS) {
                case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                    texture.setAddressMode(TinyTexture::AddressMode::ClampToEdge);
                    break;

                case TINYGLTF_TEXTURE_WRAP_REPEAT:
                case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT: // Fall back
                default:
                    texture.setAddressMode(TinyTexture::AddressMode::Repeat);
                    break;
            }
        }
        
        result.textures.push_back(std::move(texture));
    }

    result.materials.reserve(model.materials.size());
    for (const auto& gltfMaterial : model.materials) {
        TinyMaterial material;

        int albedoTexIndex = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;
        if (albedoTexIndex >= 0 && albedoTexIndex < static_cast<int>(result.textures.size())) {
            uint32_t albedoTexHash = result.textures[albedoTexIndex].hash;
            material.setAlbedoTexture(albedoTexIndex, albedoTexHash);
        }
    
        // Handle normal texture (only if textures are also being loaded)
        int normalTexIndex = gltfMaterial.normalTexture.index;
        if (normalTexIndex >= 0 && normalTexIndex < static_cast<int>(result.textures.size())) {
            uint32_t normalTexHash = result.textures[normalTexIndex].hash;
            material.setNormalTexture(normalTexIndex, normalTexHash);
        }

        result.materials.push_back(material);
    }

    // Build skeleton only if requested
    UnorderedMap<int, int> nodeIndexToBoneIndex;

    bool hasRigging = !forceStatic && !model.skins.empty();
    if (hasRigging) {
        const tinygltf::Skin& skin = model.skins[0];

        // Create the node-to-bone mapping
        for (size_t i = 0; i < skin.joints.size(); i++) {
            nodeIndexToBoneIndex[skin.joints[i]] = static_cast<int>(i);
        }

        // Load inverse bind matrices
        std::vector<glm::mat4> inverseBindMatrices;
        if (skin.inverseBindMatrices >= 0) {
            if (!readAccessor(model, skin.inverseBindMatrices, inverseBindMatrices)) {
                throw std::runtime_error("Failed to read inverse bind matrices");
            }
        } else {
            inverseBindMatrices.resize(skin.joints.size(), glm::mat4(1.0f));
        }

        // First pass: gather bone data
        for (size_t i = 0; i < skin.joints.size(); i++) {
            int nodeIndex = skin.joints[i];

            if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size())) {
                throw std::runtime_error("Invalid joint node index: " + std::to_string(nodeIndex));
            }
            
            const auto& node = model.nodes[nodeIndex];

            TinyJoint joint;
            joint.inverseBindMatrix = inverseBindMatrices[i];
            joint.localBindTransform = makeLocalFromNode(node);

            std::string originalName = node.name.empty() ? "" : node.name;
            joint.name = TinyLoader::sanitizeAsciiz(originalName, "Bone", i);

            result.skeleton.insert(joint);
        }
        
        // Second pass: fix parent relationships (optimized)
        // Build a node-to-parent lookup table once - O(m*c) where m=nodes, c=avg children
        UnorderedMap<int, int> nodeToParent;
        for (size_t nodeIdx = 0; nodeIdx < model.nodes.size(); nodeIdx++) {
            const auto& node = model.nodes[nodeIdx];
            for (int childIdx : node.children) {
                nodeToParent[childIdx] = static_cast<int>(nodeIdx);
            }
        }
        
        // Now lookup parents in O(1) per joint - O(n) total where n=joints
        for (size_t i = 0; i < skin.joints.size(); i++) {
            int nodeIndex = skin.joints[i];
            int parentBoneIndex = -1;
            
            // Find parent node in O(1)
            auto parentIt = nodeToParent.find(nodeIndex);
            if (parentIt != nodeToParent.end()) {
                int parentNodeIndex = parentIt->second;
                
                // Check if parent is also in the skeleton
                auto boneIt = nodeIndexToBoneIndex.find(parentNodeIndex);
                if (boneIt != nodeIndexToBoneIndex.end()) {
                    parentBoneIndex = boneIt->second;
                }
            }
            
            result.skeleton.parents[i] = parentBoneIndex;
        }
    }

    hasRigging &= !result.skeleton.names.empty();

    // Create a single TinyMesh to hold all combined mesh data
    TinyMesh combinedMesh;
    std::vector<TinySubmesh> submeshRanges;

    // Temporary storage for collecting all primitives data
    struct PrimitiveData {
        std::vector<glm::vec3> positions, normals;
        std::vector<glm::vec4> tangents, weights;
        std::vector<glm::vec2> uvs;
        std::vector<glm::uvec4> joints;
        std::vector<uint32_t> indices; // Always convert to uint32_t
        int materialIndex = -1;
        size_t vertexCount = 0;
    };
    
    std::vector<PrimitiveData> allPrimitives;
    TinyMesh::IndexType largestIndexType = TinyMesh::IndexType::Uint8;
    
    // First pass: Determine the largest index type and collect primitive data
    for (size_t meshIndex = 0; meshIndex < model.meshes.size(); meshIndex++) {
        const tinygltf::Mesh& mesh = model.meshes[meshIndex];

        for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); primitiveIndex++) {
            const auto& primitive = mesh.primitives[primitiveIndex];

            PrimitiveData primData;

            bool hasPosition = readAccessorFromMap(model, primitive.attributes, "POSITION", primData.positions);
            if (!hasPosition) {
                throw std::runtime_error("Mesh[" + std::to_string(meshIndex) + "] Primitive[" + 
                                        std::to_string(primitiveIndex) + "] failed to read POSITION attribute");
            }

            readAccessorFromMap(model, primitive.attributes, "NORMAL", primData.normals);
            readAccessorFromMap(model, primitive.attributes, "TANGENT", primData.tangents);
            readAccessorFromMap(model, primitive.attributes, "TEXCOORD_0", primData.uvs);
            primData.vertexCount = primData.positions.size();
            
            // Only read skin attributes if submesh has rigging
            bool submeshHasRigging = hasRigging && 
                                    primitive.attributes.count("JOINTS_0") && 
                                    primitive.attributes.count("WEIGHTS_0");

            printf("Has rigging: %d\n", submeshHasRigging);

            if (submeshHasRigging) {
                if (!readJointIndices(model, primitive.attributes.at("JOINTS_0"), primData.joints)) {
                    throw std::runtime_error("Mesh[" + std::to_string(meshIndex) + "] Primitive[" + 
                                            std::to_string(primitiveIndex) + "] failed to read joint indices");
                }

                if (!readAccessor(model, primitive.attributes.at("WEIGHTS_0"), primData.weights)) {
                    throw std::runtime_error("Mesh[" + std::to_string(meshIndex) + "] Primitive[" + 
                                            std::to_string(primitiveIndex) + "] failed to read bone weights");
                }
            }

            // Handle indices and determine largest type
            if (primitive.indices >= 0) {
                const auto& indexAccessor = model.accessors[primitive.indices];
                const auto& indexBufferView = model.bufferViews[indexAccessor.bufferView];
                const auto& indexBuffer = model.buffers[indexBufferView.buffer];
                const unsigned char* dataPtr = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;
                size_t stride = indexAccessor.ByteStride(indexBufferView);

                // Determine index type and update largest
                TinyMesh::IndexType currentType;
                switch (indexAccessor.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        currentType = TinyMesh::IndexType::Uint8;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        currentType = TinyMesh::IndexType::Uint16;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        currentType = TinyMesh::IndexType::Uint32;
                        break;
                    default:
                        throw std::runtime_error("Mesh[" + std::to_string(meshIndex) + "] Primitive[" + 
                                                std::to_string(primitiveIndex) + "] unsupported index component type");
                }
                
                // Update largest index type (Uint32 > Uint16 > Uint8)
                if (currentType > largestIndexType) {
                    largestIndexType = currentType;
                }

                // Read indices and convert to uint32_t for consistency
                primData.indices.reserve(indexAccessor.count);
                switch (indexAccessor.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                        for (size_t i = 0; i < indexAccessor.count; i++) {
                            uint8_t index = *((uint8_t*)(dataPtr + stride * i));
                            primData.indices.push_back(static_cast<uint32_t>(index));
                        }
                        break;
                    }
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                        for (size_t i = 0; i < indexAccessor.count; i++) {
                            uint16_t index = *((uint16_t*)(dataPtr + stride * i));
                            primData.indices.push_back(static_cast<uint32_t>(index));
                        }
                        break;
                    }
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                        for (size_t i = 0; i < indexAccessor.count; i++) {
                            uint32_t index = *((uint32_t*)(dataPtr + stride * i));
                            primData.indices.push_back(index);
                        }
                        break;
                    }
                }
            }

            // Set material index
            if (primitive.material >= 0 && primitive.material < static_cast<int>(result.materials.size())) {
                primData.materialIndex = primitive.material;
            }

            allPrimitives.push_back(std::move(primData));
        }
    }

    // Second pass: Combine all vertices and indices into a single TinyMesh
    if (!allPrimitives.empty()) {
        combinedMesh.indexType = largestIndexType;
        
        // Combine all vertices into continuous buffer
        if (hasRigging) {
            std::vector<TinyVertexRig> allVertices;
            size_t totalVertices = 0;
            for (const auto& primData : allPrimitives) {
                totalVertices += primData.vertexCount;
            }
            allVertices.reserve(totalVertices);
            
            // Combine all indices and create submesh ranges
            std::vector<uint32_t> allIndices;
            size_t totalIndices = 0;
            for (const auto& primData : allPrimitives) {
                totalIndices += primData.indices.size();
            }
            allIndices.reserve(totalIndices);
            
            uint32_t currentVertexOffset = 0;
            uint32_t currentIndexOffset = 0;
            
            for (const auto& primData : allPrimitives) {
                // Create vertices for this primitive
                for (size_t i = 0; i < primData.vertexCount; i++) {
                    TinyVertexRig vertex{};

                    glm::vec3 pos = primData.positions.size() > i ? primData.positions[i] : glm::vec3(0.0f);
                    glm::vec3 nrml = primData.normals.size() > i ? primData.normals[i] : glm::vec3(0.0f);
                    glm::vec2 texUV = primData.uvs.size() > i ? primData.uvs[i] : glm::vec2(0.0f);
                    glm::vec4 tang = primData.tangents.size() > i ? primData.tangents[i] : glm::vec4(1,0,0,1);

                    vertex.setPosition(pos).setNormal(nrml).setTextureUV(texUV).setTangent(tang);

                    if (primData.joints.size() > i && primData.weights.size() > i) {
                        glm::uvec4 jointIds = primData.joints[i];
                        glm::vec4 boneWeights = primData.weights[i];

                        // Validate joint indices are within skeleton range
                        bool hasInvalidJoint = false;
                        for (int j = 0; j < 4; j++) {
                            if (boneWeights[j] > 0.0f && jointIds[j] >= result.skeleton.names.size()) {
                                hasInvalidJoint = true;
                                break;
                            }
                        }

                        float weightSum = boneWeights.x + boneWeights.y + boneWeights.z + boneWeights.w;

                        if (!hasInvalidJoint && weightSum > 0.0f) {
                            // Normalize weights
                            boneWeights /= weightSum;
                            vertex.setBoneIDs(jointIds).setWeights(boneWeights);
                        }
                    }

                    allVertices.push_back(vertex);
                }
                
                // Add indices with vertex offset
                for (uint32_t index : primData.indices) {
                    allIndices.push_back(index + currentVertexOffset);
                }
                
                // Create submesh range
                TinySubmesh submesh;
                submesh.indexOffset = currentIndexOffset;
                submesh.indexCount = static_cast<uint32_t>(primData.indices.size());
                submesh.materialIndex = primData.materialIndex;
                submeshRanges.push_back(submesh);
                
                currentVertexOffset += static_cast<uint32_t>(primData.vertexCount);
                currentIndexOffset += static_cast<uint32_t>(primData.indices.size());
            }
            
            combinedMesh.setVertices(allVertices);
            
            // Convert indices to the determined largest type
            switch (largestIndexType) {
                case TinyMesh::IndexType::Uint8: {
                    std::vector<uint8_t> indices8;
                    indices8.reserve(allIndices.size());
                    for (uint32_t idx : allIndices) {
                        indices8.push_back(static_cast<uint8_t>(idx));
                    }
                    combinedMesh.setIndices(indices8);
                    break;
                }
                case TinyMesh::IndexType::Uint16: {
                    std::vector<uint16_t> indices16;
                    indices16.reserve(allIndices.size());
                    for (uint32_t idx : allIndices) {
                        indices16.push_back(static_cast<uint16_t>(idx));
                    }
                    combinedMesh.setIndices(indices16);
                    break;
                }
                case TinyMesh::IndexType::Uint32: {
                    combinedMesh.setIndices(allIndices);
                    break;
                }
            }
        } else {
            // Static vertices (no rigging)
            std::vector<TinyVertexStatic> allVertices;
            size_t totalVertices = 0;
            for (const auto& primData : allPrimitives) {
                totalVertices += primData.vertexCount;
            }
            allVertices.reserve(totalVertices);
            
            // Combine all indices and create submesh ranges
            std::vector<uint32_t> allIndices;
            size_t totalIndices = 0;
            for (const auto& primData : allPrimitives) {
                totalIndices += primData.indices.size();
            }
            allIndices.reserve(totalIndices);
            
            uint32_t currentVertexOffset = 0;
            uint32_t currentIndexOffset = 0;
            
            for (const auto& primData : allPrimitives) {
                // Create vertices for this primitive
                for (size_t i = 0; i < primData.vertexCount; i++) {
                    TinyVertexStatic vertex{};

                    glm::vec3 pos = primData.positions.size() > i ? primData.positions[i] : glm::vec3(0.0f);
                    glm::vec3 nrml = primData.normals.size() > i ? primData.normals[i] : glm::vec3(0.0f);
                    glm::vec2 texUV = primData.uvs.size() > i ? primData.uvs[i] : glm::vec2(0.0f);
                    glm::vec4 tang = primData.tangents.size() > i ? primData.tangents[i] : glm::vec4(1,0,0,1);

                    vertex.setPosition(pos).setNormal(nrml).setTextureUV(texUV).setTangent(tang);
                    allVertices.push_back(vertex);
                }
                
                // Add indices with vertex offset
                for (uint32_t index : primData.indices) {
                    allIndices.push_back(index + currentVertexOffset);
                }
                
                // Create submesh range
                TinySubmesh submesh;
                submesh.indexOffset = currentIndexOffset;
                submesh.indexCount = static_cast<uint32_t>(primData.indices.size());
                submesh.materialIndex = primData.materialIndex;
                submeshRanges.push_back(submesh);
 
                currentVertexOffset += static_cast<uint32_t>(primData.vertexCount);
                currentIndexOffset += static_cast<uint32_t>(primData.indices.size());
            }
            
            combinedMesh.setVertices(allVertices);
            
            // Convert indices to the determined largest type
            switch (largestIndexType) {
                case TinyMesh::IndexType::Uint8: {
                    std::vector<uint8_t> indices8;
                    indices8.reserve(allIndices.size());
                    for (uint32_t idx : allIndices) {
                        indices8.push_back(static_cast<uint8_t>(idx));
                    }
                    combinedMesh.setIndices(indices8);
                    break;
                }
                case TinyMesh::IndexType::Uint16: {
                    std::vector<uint16_t> indices16;
                    indices16.reserve(allIndices.size());
                    for (uint32_t idx : allIndices) {
                        indices16.push_back(static_cast<uint16_t>(idx));
                    }
                    combinedMesh.setIndices(indices16);
                    break;
                }
                case TinyMesh::IndexType::Uint32: {
                    combinedMesh.setIndices(allIndices);
                    break;
                }
            }
        }
        
        // Set submesh ranges in the combined mesh
        combinedMesh.setSubmeshes(submeshRanges);
        
        // Add the combined mesh to the result
        result.mesh = std::move(combinedMesh);
    }

    // Load animations only if skeleton is loaded and animations exist
    hasRigging &= !model.animations.empty();

    if (hasRigging) {
        result.animations.reserve(model.animations.size());

        for (size_t animIndex = 0; animIndex < model.animations.size(); animIndex++) {
            const tinygltf::Animation& gltfAnim = model.animations[animIndex];
            TinyAnimation tinyAnim;
            
            // Set animation name (use index as fallback)
            tinyAnim.name = gltfAnim.name.empty() ? 
                ("Animation_" + std::to_string(animIndex)) : gltfAnim.name;
            
            // Load samplers first
            tinyAnim.samplers.reserve(gltfAnim.samplers.size());
            for (const auto& gltfSampler : gltfAnim.samplers) {
                TinyAnimationSampler sampler;
                
                // Read time values (input)
                if (gltfSampler.input >= 0) {
                    if (!readAccessor(model, gltfSampler.input, sampler.inputTimes)) {
                        std::cerr << "Warning: Failed to read animation sampler input times for animation: " 
                                  << tinyAnim.name << std::endl;
                        continue;
                    }
                }
                
                // Set interpolation type
                if (gltfSampler.interpolation == "STEP") {
                    sampler.interpolation = TinyAnimationSampler::InterpolationType::Step;
                } else if (gltfSampler.interpolation == "CUBICSPLINE") {
                    sampler.interpolation = TinyAnimationSampler::InterpolationType::CubicSpline;
                } else {
                    sampler.interpolation = TinyAnimationSampler::InterpolationType::Linear;
                }
                
                // Note: We don't read output values here since we don't know the target path yet
                // The output will be read when processing channels
                
                tinyAnim.samplers.push_back(std::move(sampler));
            }
            
            // Load channels and read appropriate output data
            tinyAnim.channels.reserve(gltfAnim.channels.size());
            for (const auto& gltfChannel : gltfAnim.channels) {
                TinyAnimationChannel channel;
                
                // Set sampler index
                channel.samplerIndex = gltfChannel.sampler;
                if (channel.samplerIndex < 0 || channel.samplerIndex >= static_cast<int>(tinyAnim.samplers.size())) {
                    std::cerr << "Warning: Invalid sampler index in animation channel: " 
                              << tinyAnim.name << std::endl;
                    continue;
                }
                
                // Find target bone index
                if (gltfChannel.target_node >= 0) {
                    auto it = nodeIndexToBoneIndex.find(gltfChannel.target_node);
                    if (it != nodeIndexToBoneIndex.end()) {
                        channel.targetBoneIndex = it->second;
                    } else {
                        // Node is not part of the skeleton - skip this channel
                        std::cerr << "Warning: Animation channel targets node not in skeleton: " 
                                  << gltfChannel.target_node << " in animation: " << tinyAnim.name << std::endl;
                        continue;
                    }
                }
                
                // Set target path and read corresponding output data
                TinyAnimationSampler& sampler = tinyAnim.samplers[channel.samplerIndex];
                const tinygltf::AnimationSampler& gltfSampler = gltfAnim.samplers[channel.samplerIndex];
                
                if (gltfChannel.target_path == "translation") {
                    channel.targetPath = TinyAnimationChannel::TargetPath::Translation;
                    if (gltfSampler.output >= 0) {
                        if (!readAccessor(model, gltfSampler.output, sampler.translations)) {
                            std::cerr << "Warning: Failed to read translation data for animation: " 
                                      << tinyAnim.name << std::endl;
                            continue;
                        }
                    }
                } else if (gltfChannel.target_path == "rotation") {
                    channel.targetPath = TinyAnimationChannel::TargetPath::Rotation;
                    if (gltfSampler.output >= 0) {
                        if (!readAccessor(model, gltfSampler.output, sampler.rotations)) {
                            std::cerr << "Warning: Failed to read rotation data for animation: " 
                                      << tinyAnim.name << std::endl;
                            continue;
                        }
                    }
                } else if (gltfChannel.target_path == "scale") {
                    channel.targetPath = TinyAnimationChannel::TargetPath::Scale;
                    if (gltfSampler.output >= 0) {
                        if (!readAccessor(model, gltfSampler.output, sampler.scales)) {
                            std::cerr << "Warning: Failed to read scale data for animation: " 
                                      << tinyAnim.name << std::endl;
                            continue;
                        }
                    }
                } else if (gltfChannel.target_path == "weights") {
                    channel.targetPath = TinyAnimationChannel::TargetPath::Weights;
                    if (gltfSampler.output >= 0) {
                        if (!readAccessor(model, gltfSampler.output, sampler.weights)) {
                            std::cerr << "Warning: Failed to read weights data for animation: " 
                                      << tinyAnim.name << std::endl;
                            continue;
                        }
                    }
                } else {
                    std::cerr << "Warning: Unsupported animation target path: " 
                              << gltfChannel.target_path << " in animation: " << tinyAnim.name << std::endl;
                    continue;
                }
                
                tinyAnim.channels.push_back(std::move(channel));
            }
            
            // Compute animation duration and add to name mapping
            tinyAnim.computeDuration();
            result.nameToAnimationIndex[tinyAnim.name] = static_cast<int>(result.animations.size());
            result.animations.push_back(std::move(tinyAnim));
        }
    }

    return result;
}

// OBJ loader implementation using tiny_obj_loader
TinyModel TinyLoader::loadModelFromOBJ(const std::string& filePath, bool forceStatic) {

    return TinyModel(); // OBJ loading not implemented yet
}


TinyModel TinyLoader::loadModel(const std::string& filePath, bool forceStatic) {
    std::string ext;
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = filePath.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
    }

    if (ext == ".gltf" || ext == ".glb") {
        return loadModelFromGLTF(filePath, forceStatic);
    } else if (ext == ".obj") {
        return loadModelFromOBJ(filePath, forceStatic);
    } else {
        return TinyModel(); // Unsupported format
    }
}















// New implemetation

void loadTextures(std::vector<TinyTexture>& textures, tinygltf::Model& model) {
    textures.clear();
    textures.reserve(model.textures.size());

    for (const auto& gltfTexture : model.textures) {
        TinyTexture texture;

        // Load image data
        if (gltfTexture.source >= 0 && gltfTexture.source < static_cast<int>(model.images.size())) {
            const auto& image = model.images[gltfTexture.source];
            texture.
                setDimensions(image.width, image.height).
                setChannels(image.component).
                setData(image.image).
                makeHash();
        }
        
        // Load sampler settings (address mode)
        texture.addressMode = TinyTexture::AddressMode::Repeat; // Default
        if (gltfTexture.sampler >= 0 && gltfTexture.sampler < static_cast<int>(model.samplers.size())) {
            const auto& sampler = model.samplers[gltfTexture.sampler];
            
            // Convert GLTF wrap modes to our AddressMode enum
            // GLTF uses the same values for both wrapS and wrapT, so we'll use wrapS
            switch (sampler.wrapS) {
                case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                    texture.setAddressMode(TinyTexture::AddressMode::ClampToEdge);
                    break;

                case TINYGLTF_TEXTURE_WRAP_REPEAT:
                case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT: // Fall back
                default:
                    texture.setAddressMode(TinyTexture::AddressMode::Repeat);
                    break;
            }
        }
        
        textures.push_back(std::move(texture));
    }
}

struct PrimitiveData {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec4> tangents;
    std::vector<glm::uvec4> joints;
    std::vector<glm::vec4> weights;
    std::vector<uint64_t> indices; // Initial conversion
    int materialIndex = -1;
    size_t vertexCount = 0;
};

void loadMesh(TinyMesh& mesh, const tinygltf::Model& gltfModel, const std::vector<tinygltf::Primitive>& primitives, bool hasRigging) {
    std::vector<PrimitiveData> allPrimitiveDatas;

    // Shared data
    TinyMesh::IndexType largestIndexType = TinyMesh::IndexType::Uint8;

    // First pass: gather all primitive data and determine largest index type
    for (const auto& primitive : primitives) {
        PrimitiveData pd;

        if (!primitive.attributes.count("POSITION")) {
            throw std::runtime_error("Primitive missing POSITION attribute");
        }

        readAccessor(gltfModel, primitive.attributes.at("POSITION"), pd.positions);

        if (primitive.attributes.count("NORMAL")) {
            readAccessor(gltfModel, primitive.attributes.at("NORMAL"), pd.normals);
        }   
        if (primitive.attributes.count("TANGENT")) {
            readAccessor(gltfModel, primitive.attributes.at("TANGENT"), pd.tangents);
        }
        if (primitive.attributes.count("TEXCOORD_0")) {
            readAccessor(gltfModel, primitive.attributes.at("TEXCOORD_0"), pd.uvs);
        }

        if (hasRigging && primitive.attributes.count("JOINTS_0") && primitive.attributes.count("WEIGHTS_0")){
            readJointIndices(gltfModel, primitive.attributes.at("JOINTS_0"), pd.joints);
            readAccessor(gltfModel, primitive.attributes.at("WEIGHTS_0"), pd.weights);
        }

        pd.vertexCount = pd.positions.size();

        if (primitive.indices <= 0) continue;

        const auto& indexAccessor = gltfModel.accessors[primitive.indices];
        const auto& indexBufferView = gltfModel.bufferViews[indexAccessor.bufferView];
        const auto& indexBuffer = gltfModel.buffers[indexBufferView.buffer];
        const unsigned char* dataPtr = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;
        size_t stride = indexAccessor.ByteStride(indexBufferView);

        // Do 2 things: find the current index type, and append indices to pd.indices
        TinyMesh::IndexType currentType;
        switch (indexAccessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: 
                for (size_t i = 0; i < indexAccessor.count; i++) {
                    uint8_t index = *((uint8_t*)(dataPtr + stride * i));
                    pd.indices.push_back(static_cast<uint64_t>(index));
                }

                currentType = TinyMesh::IndexType::Uint8;
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                for (size_t i = 0; i < indexAccessor.count; i++) {
                    uint16_t index = *((uint16_t*)(dataPtr + stride * i));
                    pd.indices.push_back(static_cast<uint64_t>(index));
                }

                currentType = TinyMesh::IndexType::Uint16;
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                for (size_t i = 0; i < indexAccessor.count; i++) {
                    uint32_t index = *((uint32_t*)(dataPtr + stride * i));
                    pd.indices.push_back(static_cast<uint64_t>(index));
                }

                currentType = TinyMesh::IndexType::Uint32;
                break;
            default:
                throw std::runtime_error("Unsupported index component type");
        }

        // Update largest index type
        if (currentType > largestIndexType) largestIndexType = currentType;

        pd.materialIndex = primitive.material;
    }

    // Second pass: construct the TinyMesh
    if (allPrimitiveDatas.empty()) throw std::runtime_error("What kind of shitty mesh did you give me?");

    mesh.indexType = largestIndexType;
}

void loadMeshes(std::vector<TinyMesh>& meshes, tinygltf::Model& gltfModel, bool forceStatic) {
    meshes.clear();

    for (size_t meshIndex = 0; meshIndex < gltfModel.meshes.size(); meshIndex++) {
        const tinygltf::Mesh& gltfMesh = gltfModel.meshes[meshIndex];
        TinyMesh tinyMesh;

        loadMesh(tinyMesh, gltfModel, gltfMesh.primitives, !forceStatic);
        meshes.push_back(std::move(tinyMesh));
    }
}


TinyModelNew TinyLoader::loadModelFromGLTFNew(const std::string& filePath, bool forceStatic) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    loader.SetImageLoader(LoadImageData, nullptr);
    loader.SetPreserveImageChannels(true);  // Preserve original channel count

    TinyModelNew result;

    bool ok;
    if (filePath.find(".glb") != std::string::npos) {
        ok = loader.LoadBinaryFromFile(&model, &err, &warn, filePath);  // GLB
    } else {
        ok = loader.LoadASCIIFromFile(&model, &err, &warn, filePath);  // GLTF
    }

    if (!ok || model.meshes.empty()) return result;

    loadTextures(result.textures, model);
    loadMeshes(result.meshes, model, forceStatic);

    return result;
}