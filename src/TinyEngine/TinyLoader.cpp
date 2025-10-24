#include "TinyEngine/TinyLoader.hpp"
#include ".ext/Templates.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include ".ext/tiny3d/stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include ".ext/tiny3d/tiny_obj_loader.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE  
#include ".ext/tiny3d/tiny_gltf.h"

#include <array>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <unordered_map>
#include <map>
#include <tuple>
#include <functional>

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
        texture.name = filePath;
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

    if (stride == 0) stride = sizeof(T);

    out.resize(accessor.count);

    for (size_t i = 0; i < accessor.count; i++) {
        const void* ptr = dataPtr + stride * i;
        memcpy(&out[i], ptr, sizeof(T));
    }
    
    return true;
}

// Template specialization for joint indices - handles component type conversion
template<>
bool readAccessor<glm::uvec4>(const tinygltf::Model& model, int accessorIndex, std::vector<glm::uvec4>& out) {
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
    
    // Calculate stride based on component type if not specified
    if (stride == 0) {
        switch (accessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  stride = 4; break;  // VEC4 of bytes
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: stride = 8; break;  // VEC4 of shorts
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   stride = 16; break; // VEC4 of ints
            default: return false;
        }
    }
    
    out.resize(accessor.count);
    
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
        
        out[i] = jointIndices;
    }
    
    return true;
}

template<typename T>
bool readAccessorFromMap(const tinygltf::Model& model, const std::map<std::string, int>& attributes, const std::string& key, std::vector<T>& out) {
    if (attributes.count(key) == 0) return false;

    return readAccessor(model, attributes.at(key), out);
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
        return loadModelFromOBJ(filePath);
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
                setName(image.name).
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

void loadMaterials(std::vector<TinyMaterial>& materials, tinygltf::Model& model, const std::vector<TinyTexture>& textures) {
    materials.clear();
    materials.reserve(model.materials.size());
    for (size_t matIndex = 0; matIndex < model.materials.size(); matIndex++) {
        const auto& gltfMaterial = model.materials[matIndex];
        TinyMaterial material;

        // Set material name from glTF
        material.name = gltfMaterial.name.empty() ? 
            TinyLoader::sanitizeAsciiz("Material", "material", matIndex) : 
            TinyLoader::sanitizeAsciiz(gltfMaterial.name, "material", matIndex);

        int albedoTexIndex = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;
        if (albedoTexIndex >= 0 && albedoTexIndex < static_cast<int>(textures.size())) {
            uint32_t albedoTexHash = textures[albedoTexIndex].hash;
            material.setAlbedoTexture(albedoTexIndex, albedoTexHash);
        }
    
        // Handle normal texture (only if textures are also being loaded)
        int normalTexIndex = gltfMaterial.normalTexture.index;
        if (normalTexIndex >= 0 && normalTexIndex < static_cast<int>(textures.size())) {
            uint32_t normalTexHash = textures[normalTexIndex].hash;
            material.setNormalTexture(normalTexIndex, normalTexHash);
        }

        materials.push_back(material);
    }
}

struct PrimitiveData {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec4> tangents;
    std::vector<glm::uvec4> boneIDs;
    std::vector<glm::vec4> weights;
    std::vector<uint32_t> indices; // Initial conversion
    int materialIndex = -1;
    size_t vertexCount = 0;
};

void loadMesh(TinyMesh& mesh, const tinygltf::Model& gltfModel, const std::vector<tinygltf::Primitive>& primitives, bool hasRigging) {
    std::vector<PrimitiveData> allPrimitiveDatas;

    // Shared data
    VkIndexType largestIndexType = VK_INDEX_TYPE_UINT8;

    // First pass: gather all primitive data and determine largest index type
    for (const auto& primitive : primitives) {
        PrimitiveData pData;

        bool hasPosition = readAccessorFromMap(gltfModel, primitive.attributes, "POSITION", pData.positions);
        if (!hasPosition) throw std::runtime_error("Primitive failed to read POSITION attribute");

        readAccessorFromMap(gltfModel, primitive.attributes, "NORMAL", pData.normals);
        readAccessorFromMap(gltfModel, primitive.attributes, "TANGENT", pData.tangents);
        readAccessorFromMap(gltfModel, primitive.attributes, "TEXCOORD_0", pData.uvs);

        if (hasRigging) {
            readAccessorFromMap(gltfModel, primitive.attributes, "JOINTS_0", pData.boneIDs);
            readAccessorFromMap(gltfModel, primitive.attributes, "WEIGHTS_0", pData.weights);
        }

        pData.vertexCount = pData.positions.size();

        if (primitive.indices <= 0) continue;

        const auto& indexAccessor = gltfModel.accessors[primitive.indices];
        const auto& indexBufferView = gltfModel.bufferViews[indexAccessor.bufferView];
        const auto& indexBuffer = gltfModel.buffers[indexBufferView.buffer];
        const unsigned char* dataPtr = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;
        size_t stride = indexAccessor.ByteStride(indexBufferView);

        auto appendIndices = [&](auto dummyType) {
            using T = decltype(dummyType);
            for (size_t i = 0; i < indexAccessor.count; i++) {
                T index;
                std::memcpy(&index, dataPtr + stride * i, sizeof(T));
                pData.indices.push_back(static_cast<uint32_t>(index));
            }
        };

        // Do 2 things: find the current index type, and append indices to pd.indices
        VkIndexType currentType;
        switch (indexAccessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: 
                appendIndices(uint8_t{});
                currentType = VK_INDEX_TYPE_UINT8;
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                appendIndices(uint16_t{});
                currentType = VK_INDEX_TYPE_UINT16;
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                appendIndices(uint32_t{});
                currentType = VK_INDEX_TYPE_UINT32;
                break;
            default:
                throw std::runtime_error("Unsupported index component type");
        }

        // Update largest index type
        if (currentType == VK_INDEX_TYPE_UINT32) {
            largestIndexType = VK_INDEX_TYPE_UINT32;
        } else if (currentType == VK_INDEX_TYPE_UINT16 && largestIndexType != VK_INDEX_TYPE_UINT32) {
            largestIndexType = VK_INDEX_TYPE_UINT16;
        }

        pData.materialIndex = primitive.material;
        allPrimitiveDatas.push_back(std::move(pData));
    }

    // Second pass: construct the TinyMesh
    if (allPrimitiveDatas.empty())
        throw std::runtime_error("What kind of shitty mesh did you give me?");

    mesh.indexType = largestIndexType;

    std::vector<uint32_t> allIndices;
    std::vector<TinyVertexRig> allVertices;
        // Use the shared version
        // If the model happen to be static, create a new vector for TinyVertexStatic

    uint32_t currentVertexOffset = 0;
    uint32_t currentIndexOffset = 0;

    for (const auto& pData : allPrimitiveDatas) {
        for (uint32_t i = 0 ; i < pData.vertexCount; ++i) {
            TinyVertexRig vertex = TinyVertexRig()
                .setPosition( pData.positions.size() > i ? pData.positions[i] : glm::vec3(0.0f))
                .setNormal(   pData.normals.size()   > i ? pData.normals[i]   : glm::vec3(0.0f))
                .setTextureUV(pData.uvs.size()       > i ? pData.uvs[i]       : glm::vec2(0.0f))
                .setTangent(  pData.tangents.size()  > i ? pData.tangents[i]  : glm::vec4(1,0,0,1));

            if (pData.boneIDs.size() > i && pData.weights.size() > i) vertex
                .setBoneIDs(pData.boneIDs[i])
                .setWeights(pData.weights[i], true);

            allVertices.push_back(vertex);
            mesh.updateAABB(vertex.getPosition());
        }

        // Add indices with vertex offset
        for (uint32_t index : pData.indices) {
            allIndices.push_back(index + currentVertexOffset);
        }

        // Create submesh range
        TinySubmesh submesh;
        submesh.indexOffset = currentIndexOffset;
        submesh.indexCount = static_cast<uint32_t>(pData.indices.size());
        submesh.material = pData.materialIndex >= 0 ? TinyHandle(pData.materialIndex) : TinyHandle();
        mesh.addSubmesh(submesh);

        currentVertexOffset += static_cast<uint32_t>(pData.vertexCount);
        currentIndexOffset += static_cast<uint32_t>(pData.indices.size());
    }

    switch (largestIndexType) {
        case VK_INDEX_TYPE_UINT8: {
            std::vector<uint8_t> indices8;
            indices8.reserve(allIndices.size());
            for (uint32_t idx : allIndices) {
                indices8.push_back(static_cast<uint8_t>(idx));
            }
            mesh.setIndices(indices8);
            break;
        }
        case VK_INDEX_TYPE_UINT16: {
            std::vector<uint16_t> indices16;
            indices16.reserve(allIndices.size());
            for (uint32_t idx : allIndices) {
                indices16.push_back(static_cast<uint16_t>(idx));
            }
            mesh.setIndices(indices16);
            break;
        }
        case VK_INDEX_TYPE_UINT32: {
            mesh.setIndices(allIndices);
            break;
        }
    }

    if (hasRigging) mesh.setVertices(allVertices);
    else mesh.setVertices(TinyVertexRig::makeStatic(allVertices));
}

void loadMeshes(std::vector<TinyMesh>& meshes, tinygltf::Model& gltfModel, bool forceStatic) {
    meshes.clear();

    for (size_t meshIndex = 0; meshIndex < gltfModel.meshes.size(); meshIndex++) {
        const tinygltf::Mesh& gltfMesh = gltfModel.meshes[meshIndex];
        TinyMesh tinyMesh;

        // Set mesh name from glTF
        tinyMesh.name = gltfMesh.name.empty() ? 
            TinyLoader::sanitizeAsciiz("Mesh", "mesh", meshIndex) : 
            TinyLoader::sanitizeAsciiz(gltfMesh.name, "mesh", meshIndex);

        loadMesh(tinyMesh, gltfModel, gltfMesh.primitives, !forceStatic);

        meshes.push_back(std::move(tinyMesh));
    }
}


// Animation target bones, leading to a complex reference layer

void loadSkeleton(TinySkeleton& skeleton, UnorderedMap<int, std::pair<int, int>>& gltfNodeToSkeletonAndBoneIndex, int skeletonIndex, const tinygltf::Model& model, const tinygltf::Skin& skin) {
    if (skin.joints.empty()) return;

    skeleton.clear();
    
    // Set skeleton name from skin name, or create a default name
    skeleton.name = !skin.name.empty() ? skin.name : ("Skeleton_" + std::to_string(skeletonIndex));

    // Create the node-to-bone mapping
    for (int i = 0; i < skin.joints.size(); ++i) {
        int nodeIndex = skin.joints[i];

        gltfNodeToSkeletonAndBoneIndex[nodeIndex] = {skeletonIndex, i};
    }

    // Parent mapping
    UnorderedMap<int, int> nodeToParent; // Left: child node index, Right: parent node index
    for (int nodeIdx = 0; nodeIdx < model.nodes.size(); ++nodeIdx) {
        const auto& node = model.nodes[nodeIdx];

        for (int childIdx : node.children) nodeToParent[childIdx] = nodeIdx;
    }

    std::vector<glm::mat4> skeletonInverseBindMatrices;
    bool hasInvBindMat4 = readAccessor(model, skin.inverseBindMatrices, skeletonInverseBindMatrices);
    if (!hasInvBindMat4) skeletonInverseBindMatrices.resize(skin.joints.size(), glm::mat4(1.0f)); // Compromise with identity

    for (int i = 0; i < skin.joints.size(); ++i) {
        int nodeIndex = skin.joints[i];
        if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size())) {
            throw std::runtime_error("Invalid joint node index: " + std::to_string(nodeIndex));
        }

        const auto& node = model.nodes[nodeIndex];

        TinyBone bone;
        std::string originalName = node.name.empty() ? "" : node.name;
        bone.name = TinyLoader::sanitizeAsciiz(originalName, "Bone", i);
        bone.inverseBindMatrix = skeletonInverseBindMatrices[i];
        bone.localBindTransform = makeLocalFromNode(node);

        auto parentIt = nodeToParent.find(nodeIndex);
        if (parentIt != nodeToParent.end()) {
            int parentNodeIndex = parentIt->second;

            auto boneIt = gltfNodeToSkeletonAndBoneIndex.find(parentNodeIndex);
            if (boneIt != gltfNodeToSkeletonAndBoneIndex.end()) {
                bone.parent = boneIt->second.second;
            }
        }

        skeleton.insert(bone);
    }

    // Bind children
    for (int i = 0; i < skeleton.bones.size(); ++i) {
        int parentIndex = skeleton.bones[i].parent;
        if (parentIndex >= 0 && parentIndex < skeleton.bones.size()) {
            skeleton.bones[parentIndex].children.push_back(i);
        }
    }
}

void loadSkeletons(std::vector<TinySkeleton>& skeletons, UnorderedMap<int, std::pair<int, int>>& gltfNodeToSkeletonAndBoneIndex, tinygltf::Model& model) {
    skeletons.clear();
    gltfNodeToSkeletonAndBoneIndex.clear();

    for (size_t skinIndex = 0; skinIndex < model.skins.size(); ++skinIndex) {
        const tinygltf::Skin& skin = model.skins[skinIndex];

        TinySkeleton skeleton;
        loadSkeleton(skeleton, gltfNodeToSkeletonAndBoneIndex, skinIndex, model, skin);

        skeletons.push_back(std::move(skeleton));
    }
}


// struct ChannelToSkeletonMap {
//     UnorderedMap<int, int> channelToSkeletonIndex;

//     void set(int channelIndex, int skeletonIndex) {
//         channelToSkeletonIndex[channelIndex] = skeletonIndex;
//     }
// };

// void loadAnimation(TinyAnime& animation, ChannelToSkeletonMap& channelToSkeletonMap, const tinygltf::Model& model, const tinygltf::Animation& gltfAnim, const UnorderedMap<int, std::pair<int, int>>& gltfNodeToSkeletonAndBoneIndex) {
//     animation.clear();
//     animation.name = gltfAnim.name;

//     for (const auto& gltfSampler : gltfAnim.samplers) {

//         TinyAnimeSampler sampler;

//         // Read time values
//         if (gltfSampler.input >= 0) {
//             readAccessor(model, gltfSampler.input, sampler.inputTimes);
//         }

//         // Read output generically
//         if (gltfSampler.output >= 0) {
//             readAccessor(model, gltfSampler.output, sampler.outputValues);
//         }

//         sampler.setInterpolation(gltfSampler.interpolation);
//         animation.samplers.push_back(std::move(sampler));
//     }

//     for (const auto& gltfChannel : gltfAnim.channels) {
//         TinyAnimeChannel channel;
//         channel.samplerIndex = gltfChannel.sampler;

//         auto it = gltfNodeToSkeletonAndBoneIndex.find(gltfChannel.target_node);
//         if (it != gltfNodeToSkeletonAndBoneIndex.end()) {
//             int channelIndex = animation.channels.size();

//             channelToSkeletonMap.set(channelIndex, it->second.first);
//             channel.targetIndex = it->second.second;
//         }

//         channel.setTargetPath(gltfChannel.target_path);

//         animation.channels.push_back(std::move(channel));
//     }
// }

// void loadAnimations(std::vector<TinyAnime>& animations, std::vector<ChannelToSkeletonMap>& ChannelToSkeletonMaps, tinygltf::Model& model, const UnorderedMap<int, std::pair<int, int>>& gltfNodeToSkeletonAndBoneIndex) {
//     animations.clear();
//     ChannelToSkeletonMaps.clear();

//     for (size_t animIndex = 0; animIndex < model.animations.size(); ++animIndex) {
//         const tinygltf::Animation& gltfAnim = model.animations[animIndex];

//         TinyAnime animation;
//         ChannelToSkeletonMap channelToSkeletonMap;
//         loadAnimation(animation, channelToSkeletonMap, model, gltfAnim, gltfNodeToSkeletonAndBoneIndex);

//         animations.push_back(std::move(animation));
//         ChannelToSkeletonMaps.push_back(std::move(channelToSkeletonMap));
//     }
// }


void loadNodes(TinyModel& tinyModel, std::vector<int>& gltfNodeToModelNode, const tinygltf::Model& model, const UnorderedMap<int, std::pair<int, int>>& gltfNodeToSkeletonAndBoneIndex) {
    std::vector<TinyNode> nodes;

    auto pushNode = [&](TinyNode&& node) -> int {
        int index = static_cast<int>(nodes.size());
        nodes.push_back(std::move(node));
        return index;
    };

    TinyNode trueRoot;
    trueRoot.add<TinyNode::Transform>();
    trueRoot.name = tinyModel.name.empty() ? "Model_Root" : tinyModel.name;
    pushNode(std::move(trueRoot));

    // Construct all root-level nodes first

    // Add a single node to store all skeletons
    TinyNode skeletonRootNode("LSkeletons");
    int skeletonRootModelIndex = pushNode(std::move(skeletonRootNode));

    nodes[0].addChild(TinyHandle(skeletonRootModelIndex));
    nodes[skeletonRootModelIndex].setParent(TinyHandle(0));

    // Skeleton parent nodes
    UnorderedMap<int, int> skeletonToModelNodeIndex;
    for (size_t skeleIdx = 0; skeleIdx < tinyModel.skeletons.size(); ++skeleIdx) {
        // We only need this skeleton for the name
        const TinySkeleton& skeleton = tinyModel.skeletons[skeleIdx];
        TinyNode skeleNode(skeleton.name);

        TinyNode::Skeleton skeleComp;
        skeleComp.pSkeleHandle = TinyHandle(skeleIdx);

        skeleNode.add<TinyNode::Skeleton>(std::move(skeleComp));

        int skeleNodeIndex = pushNode(std::move(skeleNode));
        skeletonToModelNodeIndex[(int)skeleIdx] = skeleNodeIndex;

        // Child of skeleton root
        nodes[skeletonRootModelIndex].addChild(TinyHandle(skeleNodeIndex)); 
        nodes[skeleNodeIndex].setParent(TinyHandle(skeletonRootModelIndex));
    }

    std::vector<int> gltfNodeParent(model.nodes.size(), -1);
    std::vector<bool> gltfNodeIsJoint(model.nodes.size(), false);
    gltfNodeToModelNode.resize(model.nodes.size(), -1);

    for (int i = 0; i < model.nodes.size(); ++i) {
        for (int childIdx : model.nodes[i].children) {
            gltfNodeParent[childIdx] = i;
        }

        if (gltfNodeToSkeletonAndBoneIndex.find(i) !=
            gltfNodeToSkeletonAndBoneIndex.end()) {
            gltfNodeIsJoint[i] = true;
            continue; // It's a joint node -> skip
        }

        TinyNode emptyNode;
        emptyNode.name = model.nodes[i].name.empty() ? "Node" : model.nodes[i].name;

        int modelIdx = pushNode(std::move(emptyNode));
        gltfNodeToModelNode[i] = modelIdx;
    }

    // Second pass: parent-child wiring + setting details
    for (int i = 0; i < model.nodes.size(); ++i) {
        const tinygltf::Node& nGltf = model.nodes[i];

        int nModelIndex = gltfNodeToModelNode[i];
        if (nModelIndex < 0) continue; // skip joint

        TinyNode& nModel = nodes[nModelIndex];

        // Establish parent-child relationship
        int parentGltfIndex = gltfNodeParent[i];
        int parentModelIndex = -1;
        if (parentGltfIndex >= 0) {
            parentModelIndex = gltfNodeToModelNode[parentGltfIndex];
            parentModelIndex = parentModelIndex < 0 ? 0 : parentModelIndex; // Fallback to true root

            nodes[parentModelIndex].addChild(TinyHandle(nModelIndex));
            nModel.setParent(TinyHandle(parentModelIndex));
        } else {
            nodes[0].addChild(TinyHandle(nModelIndex));
            nModel.setParent(TinyHandle(0));
        }

        // Start adding components shall we?

        TinyNode::Transform* transformComp = nModel.add<TinyNode::Transform>();

        glm::mat4 matrix(1.0f);
        if (!nGltf.matrix.empty()) {
            matrix = glm::make_mat4(nGltf.matrix.data());
        } else {
            glm::vec3 translation(0.0f);
            glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
            glm::vec3 scale(1.0f);

            if (!nGltf.translation.empty())
                translation = glm::make_vec3(nGltf.translation.data());
            if (!nGltf.rotation.empty())
                rotation = glm::quat(
                    nGltf.rotation[3],
                    nGltf.rotation[0],
                    nGltf.rotation[1],
                    nGltf.rotation[2]);
            if (!nGltf.scale.empty())
                scale = glm::make_vec3(nGltf.scale.data());

            matrix= glm::translate(glm::mat4(1.0f), translation) *
                    glm::mat4_cast(rotation) *
                    glm::scale(glm::mat4(1.0f), scale);
        }
        transformComp->local = matrix;

        if (nGltf.mesh >= 0) {
            TinyNode::MeshRender meshData;
            meshData.pMeshHandle = TinyHandle(nGltf.mesh);

            int skeletonIndex = nGltf.skin;
            auto it = skeletonToModelNodeIndex.find(skeletonIndex);
            if (it != skeletonToModelNodeIndex.end()) {
                meshData.skeleNodeHandle = TinyHandle(it->second);
            }

            nModel.add<TinyNode::MeshRender>(std::move(meshData));

            // Check if the parent of this node is a joint
            auto itBone = gltfNodeToSkeletonAndBoneIndex.find(parentGltfIndex);
            if (itBone != gltfNodeToSkeletonAndBoneIndex.end()) {
                // This node is associated with a bone
                int skeleIdx = itBone->second.first;
                int boneIdx = itBone->second.second;

                auto skeleIt = skeletonToModelNodeIndex.find(skeleIdx);
                if (skeleIt != skeletonToModelNodeIndex.end()) {
                    TinyNode::BoneAttach boneAttach;
                    boneAttach.skeleNodeHandle = TinyHandle(skeleIt->second);
                    boneAttach.boneIndex = boneIdx;
                    nModel.add<TinyNode::BoneAttach>(std::move(boneAttach));
                }
            }
        }
    }

    // print the nodes (flat)
    // for (const auto& node : nodes) {
    //     printf("Node: %s, Parent: %u, Children: [ ",
    //         node.name.c_str(),
    //         node.parentHandle.index
    //     );
    //     for (const auto& childHandle : node.childrenHandles) {
    //         printf("%u ", childHandle.index);
    //     }
    //     printf("]\n");
    // }

    tinyModel.nodes = std::move(nodes);
}



TinyModel TinyLoader::loadModelFromGLTF(const std::string& filePath, bool forceStatic) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    loader.SetImageLoader(LoadImageData, nullptr);
    loader.SetPreserveImageChannels(true);  // Preserve original channel count

    bool ok;
    if (filePath.find(".glb") != std::string::npos) {
        ok = loader.LoadBinaryFromFile(&model, &err, &warn, filePath);  // GLB
    } else {
        ok = loader.LoadASCIIFromFile(&model, &err, &warn, filePath);  // GLTF
    }


    TinyModel result;

    std::string name = filePath;
    size_t slashPos = name.find_last_of("/\\");
    if (slashPos != std::string::npos) name = name.substr(slashPos + 1);
    size_t dotPos = name.find_last_of('.');
    if (dotPos != std::string::npos) name = name.substr(0, dotPos);
    result.name = name;

    if (!ok || model.meshes.empty()) return result;

    loadTextures(result.textures, model);
    loadMaterials(result.materials, model, result.textures);

    UnorderedMap<int, std::pair<int, int>> gltfNodeToSkeletonAndBoneIndex;
    if (!forceStatic) loadSkeletons(result.skeletons, gltfNodeToSkeletonAndBoneIndex, model);

    bool hasRigging = !forceStatic && !result.skeletons.empty();
    loadMeshes(result.meshes, model, !hasRigging);

    std::vector<int> gltfNodeToModelNode;
    // loadNodes(result, gltfNodeToModelNode, model, gltfNodeToSkeletonAndBoneIndex);
    loadNodes(result, gltfNodeToModelNode, model, gltfNodeToSkeletonAndBoneIndex);
        
    // loadAnimations(result, model, gltfNodeToSkeletonAndBoneIndex);

    return result;
}








// OBJ loader implementation using tiny_obj_loader
TinyModel TinyLoader::loadModelFromOBJ(const std::string& filePath) {
    TinyModel result;
    
    // Extract model name from file path
    std::string name = filePath;
    size_t slashPos = name.find_last_of("/\\");
    if (slashPos != std::string::npos) name = name.substr(slashPos + 1);
    size_t dotPos = name.find_last_of('.');
    if (dotPos != std::string::npos) name = name.substr(0, dotPos);
    result.name = name;

    // Load OBJ file using tinyobjloader
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> objMaterials;
    std::string warn, err;

    // Extract directory path for MTL file lookup
    std::string mtlDir = "";
    size_t lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        mtlDir = filePath.substr(0, lastSlash + 1);
    }

    bool success = tinyobj::LoadObj(&attrib, &shapes, &objMaterials, &warn, &err, filePath.c_str(), mtlDir.c_str());
    
    if (!warn.empty()) {
        std::cout << "Warning: " << warn << std::endl;
    }
    
    if (!err.empty()) {
        std::cerr << "Error: " << err << std::endl;
    }
    
    if (!success) {
        return result; // Return empty model on failure
    }

    // Convert OBJ materials to TinyMaterials
    result.materials.reserve(objMaterials.size());
    for (size_t matIndex = 0; matIndex < objMaterials.size(); matIndex++) {
        const auto& objMat = objMaterials[matIndex];
        TinyMaterial material;
        material.name = objMat.name.empty() ? 
            sanitizeAsciiz("Material", "material", matIndex) : 
            sanitizeAsciiz(objMat.name, "material", matIndex);

        // Load diffuse texture if present
        if (!objMat.diffuse_texname.empty()) {
            // Construct full texture path
            std::string texturePath = objMat.diffuse_texname;
            if (texturePath.find("/") == std::string::npos && texturePath.find("\\") == std::string::npos) {
                // Relative path, combine with model directory
                size_t lastSlash = filePath.find_last_of("/\\");
                if (lastSlash != std::string::npos) {
                    texturePath = filePath.substr(0, lastSlash + 1) + texturePath;
                }
            }

            TinyTexture texture = loadTexture(texturePath);
            if (texture.width > 0 && texture.height > 0) {
                // Extract just the filename for the texture name
                std::string textureName = objMat.diffuse_texname;
                size_t lastSlash = textureName.find_last_of("/\\");
                if (lastSlash != std::string::npos) {
                    textureName = textureName.substr(lastSlash + 1);
                }
                texture.setName(textureName);
                
                int width = texture.width;
                int height = texture.height;
                uint64_t textureHash = texture.hash;
                result.textures.push_back(std::move(texture));
                uint32_t textureIndex = static_cast<uint32_t>(result.textures.size() - 1);
                material.setAlbedoTexture(textureIndex, textureHash);
            }
        }

        result.materials.push_back(std::move(material));
    }

    // Group faces by material to create separate meshes per material
    std::map<int, std::vector<size_t>> materialToFaces; // material_id -> face indices

    for (const auto& shape : shapes) {
        const auto& mesh = shape.mesh;
        
        size_t faceIndex = 0;
        for (size_t i = 0; i < mesh.num_face_vertices.size(); i++) {
            int materialId = mesh.material_ids.empty() ? -1 : mesh.material_ids[i];
            materialToFaces[materialId].push_back(faceIndex);
            faceIndex += mesh.num_face_vertices[i]; // Move to next face
        }
    }

    // Create meshes for each material
    result.meshes.reserve(materialToFaces.size());
    
    for (const auto& [materialId, faceIndices] : materialToFaces) {
        TinyMesh mesh;
        
        // Create mesh name based on material
        if (materialId >= 0 && materialId < static_cast<int>(objMaterials.size())) {
            mesh.name = sanitizeAsciiz(objMaterials[materialId].name, "mesh", result.meshes.size());
        } else {
            mesh.name = "Mesh_" + std::to_string(result.meshes.size());
        }

        std::vector<TinyVertexStatic> vertices;
        std::vector<uint32_t> indices;
        
        // Custom hasher for vertex key tuple
        struct VertexKeyHasher {
            std::size_t operator()(const std::tuple<int, int, int>& t) const {
                return std::get<0>(t) ^ (std::get<1>(t) << 11) ^ (std::get<2>(t) << 22);
            }
        };
        
        std::unordered_map<std::tuple<int, int, int>, uint32_t, VertexKeyHasher> vertexMap;

        uint32_t currentVertexIndex = 0;

        // Process all shapes for this material
        for (const auto& shape : shapes) {
            const auto& objMesh = shape.mesh;
            
            size_t faceVertexIndex = 0;
            for (size_t faceIdx = 0; faceIdx < objMesh.num_face_vertices.size(); faceIdx++) {
                int faceMaterialId = objMesh.material_ids.empty() ? -1 : objMesh.material_ids[faceIdx];
                
                // Skip faces that don't belong to this material
                if (faceMaterialId != materialId) {
                    faceVertexIndex += objMesh.num_face_vertices[faceIdx];
                    continue;
                }

                unsigned int faceVertexCount = objMesh.num_face_vertices[faceIdx];
                
                // Triangulate face if necessary (convert quads+ to triangles)
                std::vector<uint32_t> faceIndices;
                
                for (unsigned int v = 0; v < faceVertexCount; v++) {
                    tinyobj::index_t idx = objMesh.indices[faceVertexIndex + v];
                    
                    // Create vertex key for deduplication
                    std::tuple<int, int, int> vertexKey = std::make_tuple(idx.vertex_index, idx.normal_index, idx.texcoord_index);
                    
                    uint32_t vertexIndex;
                    auto it = vertexMap.find(vertexKey);
                    if (it != vertexMap.end()) {
                        // Reuse existing vertex
                        vertexIndex = it->second;
                    } else {
                        // Create new vertex
                        TinyVertexStatic vertex;
                        
                        // Position
                        if (idx.vertex_index >= 0) {
                            vertex.setPosition(glm::vec3(
                                attrib.vertices[3 * idx.vertex_index + 0],
                                attrib.vertices[3 * idx.vertex_index + 1],
                                attrib.vertices[3 * idx.vertex_index + 2]
                            ));
                        }
                        
                        // Normal
                        if (idx.normal_index >= 0) {
                            vertex.setNormal(glm::vec3(
                                attrib.normals[3 * idx.normal_index + 0],
                                attrib.normals[3 * idx.normal_index + 1],
                                attrib.normals[3 * idx.normal_index + 2]
                            ));
                        }
                        
                        // Texture coordinates
                        if (idx.texcoord_index >= 0) {
                            vertex.setTextureUV(glm::vec2(
                                attrib.texcoords[2 * idx.texcoord_index + 0],
                                1.0f - attrib.texcoords[2 * idx.texcoord_index + 1] // Flip V coordinate
                            ));
                        }
                        
                        // Set default tangent (will be computed later if needed)
                        vertex.setTangent(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
                        
                        vertexIndex = currentVertexIndex++;
                        vertices.push_back(vertex);
                        vertexMap[vertexKey] = vertexIndex;
                    }
                    
                    faceIndices.push_back(vertexIndex);
                }
                
                // Convert face to triangles (fan triangulation for n > 3)
                if (faceVertexCount >= 3) {
                    for (unsigned int t = 1; t < faceVertexCount - 1; t++) {
                        indices.push_back(faceIndices[0]);
                        indices.push_back(faceIndices[t]);
                        indices.push_back(faceIndices[t + 1]);
                    }
                }
                
                faceVertexIndex += faceVertexCount;
            }
        }

        // Set mesh data
        mesh.setVertices(vertices);
        mesh.setIndices(indices);

        // Create single submesh covering entire mesh
        TinySubmesh submesh;
        submesh.indexOffset = 0;
        submesh.indexCount = static_cast<uint32_t>(indices.size());
        submesh.material = (materialId >= 0) ? TinyHandle(materialId) : TinyHandle();
        mesh.addSubmesh(submesh);

        result.meshes.push_back(std::move(mesh));
    }

    // Create scene hierarchy: Root node + one child node per mesh
    result.nodes.clear();
    result.nodes.reserve(1 + result.meshes.size());

    // Root node (index 0)
    TinyNode rootNode;
    rootNode.add<TinyNode::Transform>();
    rootNode.name = result.name.empty() ? "OBJ_Root" : result.name;
    result.nodes.push_back(std::move(rootNode));

    // Child nodes for each mesh (representing each material group)
    for (size_t meshIndex = 0; meshIndex < result.meshes.size(); meshIndex++) {
        TinyNode meshNode;
        meshNode.add<TinyNode::Transform>();
        meshNode.name = result.meshes[meshIndex].name + "_Node";

        // Set parent-child relationship
        meshNode.setParent(TinyHandle(0)); // Parent is root node
        result.nodes[0].addChild(TinyHandle(static_cast<uint32_t>(meshIndex + 1)));

        // Add MeshRender component
        TinyNode::MeshRender meshRender;
        meshRender.pMeshHandle = TinyHandle(static_cast<uint32_t>(meshIndex));
        meshRender.skeleNodeHandle = TinyHandle(); // No skeleton for OBJ
        meshNode.add<TinyNode::MeshRender>(std::move(meshRender));

        result.nodes.push_back(std::move(meshNode));
    }

    return result;
}
