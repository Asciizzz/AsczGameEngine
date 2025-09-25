#include "TinyData/TinyLoader.hpp"

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
TinyModel TinyLoader::loadModelFromGLTF(const std::string& filePath, const LoadOptions& options) {
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

    // Load textures only if requested
    if (options.loadTextures && options.loadMaterials) {
        result.textures.reserve(model.textures.size());
        for (const auto& gltfTexture : model.textures) {
            TinyTexture texture;

            // Load image data
            if (gltfTexture.source >= 0 && gltfTexture.source < static_cast<int>(model.images.size())) {
                const auto& image = model.images[gltfTexture.source];
                texture.width = image.width;
                texture.height = image.height;
                texture.channels = image.component;
                texture.data = image.image;
                texture.makeHash();
            }
            
            // Load sampler settings (address mode)
            texture.addressMode = TinyTexture::AddressMode::Repeat; // Default
            if (gltfTexture.sampler >= 0 && gltfTexture.sampler < static_cast<int>(model.samplers.size())) {
                const auto& sampler = model.samplers[gltfTexture.sampler];
                
                // Convert GLTF wrap modes to our AddressMode enum
                // GLTF uses the same values for both wrapS and wrapT, so we'll use wrapS
                switch (sampler.wrapS) {
                    case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                        texture.addressMode = TinyTexture::AddressMode::ClampToEdge;
                        break;
                    case TINYGLTF_TEXTURE_WRAP_REPEAT:
                    case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
                        // We don't have MirroredRepeat, fallback to Repeat
                        texture.addressMode = TinyTexture::AddressMode::Repeat;
                        break;
                    default:
                        texture.addressMode = TinyTexture::AddressMode::Repeat;
                        break;
                }
            }
            
            result.textures.push_back(std::move(texture));
        }
    }

    // Load materials only if requested
    if (options.loadMaterials) {
        result.materials.reserve(model.materials.size());
        for (const auto& gltfMaterial : model.materials) {
            TinyMaterial material;
            
            // Handle albedo texture (only if textures are also being loaded)
            if (options.loadTextures && gltfMaterial.pbrMetallicRoughness.baseColorTexture.index >= 0) {
                int texIndex = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;
                uint32_t texHash = result.textures[texIndex].hash;

                if (texIndex >= 0 && texIndex < static_cast<int>(result.textures.size())) {
                    material.setAlbedoTexture(texIndex, texHash);
                }
            }
            
            // Handle normal texture (only if textures are also being loaded)
            if (options.loadTextures && gltfMaterial.normalTexture.index >= 0) {
                int texIndex = gltfMaterial.normalTexture.index;
                uint32_t texHash = result.textures[texIndex].hash;

                if (texIndex >= 0 && texIndex < static_cast<int>(result.textures.size())) {
                    material.setNormalTexture(texIndex, texHash);
                }
            }
            
            result.materials.push_back(material);
        }
    }

    // Build skeleton only if requested
    UnorderedMap<int, int> nodeIndexToBoneIndex;

    bool hasRigging = !options.forceStatic && !model.skins.empty();
    if (hasRigging) {
        const tinygltf::Skin& skin = model.skins[0];

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
        } else {
            inverseBindMatrices.resize(skin.joints.size(), glm::mat4(1.0f));
        }

        // Build skeleton structure
        result.skeleton.names.reserve(skin.joints.size());
        result.skeleton.parentIndices.reserve(skin.joints.size());
        result.skeleton.inverseBindMatrices.reserve(skin.joints.size());
        result.skeleton.localBindTransforms.reserve(skin.joints.size());

        // First pass: gather bone data
        for (size_t i = 0; i < skin.joints.size(); i++) {
            int nodeIndex = skin.joints[i];
            
            if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size())) {
                throw std::runtime_error("Invalid joint node index: " + std::to_string(nodeIndex));
            }
            
            const auto& node = model.nodes[nodeIndex];
            std::string originalName = node.name.empty() ? "" : node.name;
            std::string boneName = TinyLoader::sanitizeAsciiz(originalName, "Bone", i);
            
            result.skeleton.names.push_back(boneName);
            result.skeleton.parentIndices.push_back(-1); // Will be fixed in second pass
            result.skeleton.inverseBindMatrices.push_back(inverseBindMatrices.size() > i ? inverseBindMatrices[i] : glm::mat4(1.0f));
            result.skeleton.localBindTransforms.push_back(makeLocalFromNode(node));
            result.skeleton.nameToIndex[boneName] = static_cast<int>(i);
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
            
            result.skeleton.parentIndices[i] = parentBoneIndex;
        }
    }

    hasRigging &= !result.skeleton.names.empty();

    // Create a single TinyMesh to hold all combined mesh data
    TinyMesh combinedMesh;
    std::vector<TinySubmesh> submeshRanges;
    std::vector<int> submeshMaterials;
    
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

            // Validate required attributes
            if (!primitive.attributes.count("POSITION")) {
                throw std::runtime_error("Mesh[" + std::to_string(meshIndex) + "] Primitive[" + 
                                        std::to_string(primitiveIndex) + "] missing POSITION attribute");
            }
            
            PrimitiveData primData;
            primData.vertexCount = model.accessors[primitive.attributes.at("POSITION")].count;

            // Read standard attributes with validation
            readAccessor(model, model.accessors[primitive.attributes.at("POSITION")], primData.positions);
            
            if (primitive.attributes.count("NORMAL")) {
                readAccessor(model, model.accessors[primitive.attributes.at("NORMAL")], primData.normals);
            }
            if (primitive.attributes.count("TANGENT")) {
                readAccessor(model, model.accessors[primitive.attributes.at("TANGENT")], primData.tangents);
            }
            if (primitive.attributes.count("TEXCOORD_0")) {
                readAccessor(model, model.accessors[primitive.attributes.at("TEXCOORD_0")], primData.uvs);
            }
            
            // Only read skin attributes if submesh has rigging
            bool submeshHasRigging = hasRigging && 
                                    primitive.attributes.count("JOINTS_0") && 
                                    primitive.attributes.count("WEIGHTS_0");
            if (submeshHasRigging) {
                if (!readJointIndices(model, primitive.attributes.at("JOINTS_0"), primData.joints)) {
                    throw std::runtime_error("Mesh[" + std::to_string(meshIndex) + "] Primitive[" + 
                                            std::to_string(primitiveIndex) + "] failed to read joint indices");
                }

                if (!readAccessorSafe(model, primitive.attributes.at("WEIGHTS_0"), primData.weights)) {
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
            if (options.loadMaterials && primitive.material >= 0 && primitive.material < static_cast<int>(result.materials.size())) {
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
                submeshRanges.push_back(submesh);
                submeshMaterials.push_back(primData.materialIndex);
                
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
                submeshRanges.push_back(submesh);
                submeshMaterials.push_back(primData.materialIndex);
                
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
        result.submeshMaterials = std::move(submeshMaterials);
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
                    if (!readAccessorSafe(model, gltfSampler.input, sampler.inputTimes)) {
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
                        if (!readAccessorSafe(model, gltfSampler.output, sampler.translations)) {
                            std::cerr << "Warning: Failed to read translation data for animation: " 
                                      << tinyAnim.name << std::endl;
                            continue;
                        }
                    }
                } else if (gltfChannel.target_path == "rotation") {
                    channel.targetPath = TinyAnimationChannel::TargetPath::Rotation;
                    if (gltfSampler.output >= 0) {
                        if (!readAccessorSafe(model, gltfSampler.output, sampler.rotations)) {
                            std::cerr << "Warning: Failed to read rotation data for animation: " 
                                      << tinyAnim.name << std::endl;
                            continue;
                        }
                    }
                } else if (gltfChannel.target_path == "scale") {
                    channel.targetPath = TinyAnimationChannel::TargetPath::Scale;
                    if (gltfSampler.output >= 0) {
                        if (!readAccessorSafe(model, gltfSampler.output, sampler.scales)) {
                            std::cerr << "Warning: Failed to read scale data for animation: " 
                                      << tinyAnim.name << std::endl;
                            continue;
                        }
                    }
                } else if (gltfChannel.target_path == "weights") {
                    channel.targetPath = TinyAnimationChannel::TargetPath::Weights;
                    if (gltfSampler.output >= 0) {
                        if (!readAccessorSafe(model, gltfSampler.output, sampler.weights)) {
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
TinyModel TinyLoader::loadModelFromOBJ(const std::string& filePath, const LoadOptions& options) {

    return TinyModel(); // OBJ loading not implemented yet
}



TinyModel TinyLoader::loadModel(const std::string& filePath, const LoadOptions& options) {
    std::string ext;
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = filePath.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
    }

    if (ext == ".gltf" || ext == ".glb") {
        return loadModelFromGLTF(filePath, options);
    } else if (ext == ".obj") {
        return loadModelFromOBJ(filePath, options);
    } else {
        return TinyModel(); // Unsupported format
    }
}


