#include "TinyEngine/TinyLoader.hpp"

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

void loadMaterials(std::vector<TinyMaterial>& materials, tinygltf::Model& model, const std::vector<TinyTexture>& textures) {
    materials.clear();
    materials.reserve(model.materials.size());
    for (const auto& gltfMaterial : model.materials) {
        TinyMaterial material;

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

void loadMesh(TinyMesh& mesh, std::vector<TinyHandle>& submeshMats, const tinygltf::Model& gltfModel, const std::vector<tinygltf::Primitive>& primitives, bool hasRigging) {
    std::vector<PrimitiveData> allPrimitiveDatas;

    // Shared data
    TinyMesh::IndexType largestIndexType = TinyMesh::IndexType::Uint8;

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
        TinyMesh::IndexType currentType;
        switch (indexAccessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: 
                appendIndices(uint8_t{});
                currentType = TinyMesh::IndexType::Uint8;
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                appendIndices(uint16_t{});
                currentType = TinyMesh::IndexType::Uint16;
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                appendIndices(uint32_t{});
                currentType = TinyMesh::IndexType::Uint32;
                break;
            default:
                throw std::runtime_error("Unsupported index component type");
        }

        // Update largest index type
        if (currentType > largestIndexType) largestIndexType = currentType;

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
        }

        // Add indices with vertex offset
        for (uint32_t index : pData.indices) {
            allIndices.push_back(index + currentVertexOffset);
        }

        // Create submesh range
        TinySubmesh submesh;
        submesh.indexOffset = currentIndexOffset;
        submesh.indexCount = static_cast<uint32_t>(pData.indices.size());
        mesh.addSubmesh(submesh);

        TinyHandle matHandle; // Invalid by default
        if (pData.materialIndex >= 0) {
            matHandle = TinyHandle(pData.materialIndex);
        }

        submeshMats.push_back(matHandle);

        currentVertexOffset += static_cast<uint32_t>(pData.vertexCount);
        currentIndexOffset += static_cast<uint32_t>(pData.indices.size());
    }

    switch (largestIndexType) {
        case TinyMesh::IndexType::Uint8: {
            std::vector<uint8_t> indices8;
            indices8.reserve(allIndices.size());
            for (uint32_t idx : allIndices) {
                indices8.push_back(static_cast<uint8_t>(idx));
            }
            mesh.setIndices(indices8);
            break;
        }
        case TinyMesh::IndexType::Uint16: {
            std::vector<uint16_t> indices16;
            indices16.reserve(allIndices.size());
            for (uint32_t idx : allIndices) {
                indices16.push_back(static_cast<uint16_t>(idx));
            }
            mesh.setIndices(indices16);
            break;
        }
        case TinyMesh::IndexType::Uint32: {
            mesh.setIndices(allIndices);
            break;
        }
    }

    if (hasRigging) mesh.setVertices(allVertices);
    else mesh.setVertices(TinyVertexRig::makeStaticVertices(allVertices));
}

void loadMeshes(std::vector<TinyMesh>& meshes, std::vector<std::vector<TinyHandle>>& meshesMaterials, tinygltf::Model& gltfModel, bool forceStatic) {
    meshes.clear();
    meshesMaterials.clear();

    for (size_t meshIndex = 0; meshIndex < gltfModel.meshes.size(); meshIndex++) {
        const tinygltf::Mesh& gltfMesh = gltfModel.meshes[meshIndex];
        TinyMesh tinyMesh;
        std::vector<TinyHandle> submeshMats;

        loadMesh(tinyMesh, submeshMats, gltfModel, gltfMesh.primitives, !forceStatic);

        meshes.push_back(std::move(tinyMesh));
        meshesMaterials.push_back(std::move(submeshMats));
    }
}


// For legacy support - combines all meshes into one
void loadMeshCombined(TinyMesh& mesh, std::vector<int>& submeshMats, tinygltf::Model& gltfModel, bool forceStatic) {
    // Combined primitives
    std::vector<tinygltf::Primitive> combinedPrimitives;
    for (const auto& gltfMesh : gltfModel.meshes) {
        combinedPrimitives.insert(combinedPrimitives.end(), gltfMesh.primitives.begin(), gltfMesh.primitives.end());
    }

    std::vector<TinyHandle> submeshMatsHandles;
    loadMesh(mesh, submeshMatsHandles, gltfModel, combinedPrimitives, !forceStatic);

    // Reconvert handles to indices (or -1)
    submeshMats.clear();
    for (const auto& handle : submeshMatsHandles) {
        if (handle.isValid()) submeshMats.push_back(handle.index);
        else                  submeshMats.push_back(-1);
    }
}

// Animation target bones, leading to a complex reference layer

void loadSkeleton(TinySkeleton& skeleton, UnorderedMap<int, std::pair<int, int>>& nodeToSkeletonAndBoneIndex, int skeletonIndex, const tinygltf::Model& model, const tinygltf::Skin& skin) {
    if (skin.joints.empty()) return;

    skeleton.clear();

    // Create the node-to-bone mapping
    for (int i = 0; i < skin.joints.size(); ++i) {
        int nodeIndex = skin.joints[i];

        nodeToSkeletonAndBoneIndex[nodeIndex] = {skeletonIndex, i};
    }

    // Parent mapping
    UnorderedMap<int, int> nodeToParent; // Left: child node index, Right: parent node index
    for (int nodeIdx = 0; nodeIdx < model.nodes.size(); ++nodeIdx) {
        const auto& node = model.nodes[nodeIdx];

        for (int childIdx : node.children) nodeToParent[childIdx] = nodeIdx;
    }

    bool hasInvBindMat4 = readAccessor(model, skin.inverseBindMatrices, skeleton.inverseBindMatrices);
    if (!hasInvBindMat4) skeleton.inverseBindMatrices.resize(skin.joints.size(), glm::mat4(1.0f)); // Compromise with identity

    for (int i = 0; i < skin.joints.size(); ++i) {
        int nodeIndex = skin.joints[i];
        if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size())) {
            throw std::runtime_error("Invalid joint node index: " + std::to_string(nodeIndex));
        }

        const auto& node = model.nodes[nodeIndex];

        TinyBone bone;
        std::string originalName = node.name.empty() ? "" : node.name;
        bone.name = TinyLoader::sanitizeAsciiz(originalName, "Bone", i);
        bone.inverseBindMatrix = skeleton.inverseBindMatrices[i];
        bone.localBindTransform = makeLocalFromNode(node);

        auto parentIt = nodeToParent.find(nodeIndex);
        if (parentIt != nodeToParent.end()) {
            int parentNodeIndex = parentIt->second;

            auto boneIt = nodeToSkeletonAndBoneIndex.find(parentNodeIndex);
            if (boneIt != nodeToSkeletonAndBoneIndex.end()) {
                bone.parent = boneIt->second.second;
            }
        }

        skeleton.insert(bone);
    }
}

void loadSkeletons(std::vector<TinySkeleton>& skeletons, UnorderedMap<int, std::pair<int, int>>& nodeToSkeletonAndBoneIndex, tinygltf::Model& model) {
    skeletons.clear();
    nodeToSkeletonAndBoneIndex.clear();

    for (size_t skinIndex = 0; skinIndex < model.skins.size(); ++skinIndex) {
        const tinygltf::Skin& skin = model.skins[skinIndex];

        TinySkeleton skeleton;
        loadSkeleton(skeleton, nodeToSkeletonAndBoneIndex, skinIndex, model, skin);

        skeletons.push_back(std::move(skeleton));
    }
}


struct ChannelToSkeletonMap {
    UnorderedMap<int, int> channelToSkeletonIndex;

    void set(int channelIndex, int skeletonIndex) {
        channelToSkeletonIndex[channelIndex] = skeletonIndex;
    }
};

void loadAnimation(TinyAnimation& animation, ChannelToSkeletonMap& channelToSkeletonMap, const tinygltf::Model& model, const tinygltf::Animation& gltfAnim, const UnorderedMap<int, std::pair<int, int>>& nodeToSkeletonAndBoneIndex) {
    animation.clear();
    animation.name = gltfAnim.name;

    for (const auto& gltfSampler : gltfAnim.samplers) {

        TinyAnimationSampler sampler;

        // Read time values
        if (gltfSampler.input >= 0) {
            readAccessor(model, gltfSampler.input, sampler.inputTimes);
        }

        // Read output generically
        if (gltfSampler.output >= 0) {
            readAccessor(model, gltfSampler.output, sampler.outputValues);
        }

        sampler.setInterpolation(gltfSampler.interpolation);
        animation.samplers.push_back(std::move(sampler));
    }

    for (const auto& gltfChannel : gltfAnim.channels) {
        TinyAnimationChannel channel;
        channel.samplerIndex = gltfChannel.sampler;

        auto it = nodeToSkeletonAndBoneIndex.find(gltfChannel.target_node);
        if (it != nodeToSkeletonAndBoneIndex.end()) {
            int channelIndex = animation.channels.size();

            channelToSkeletonMap.set(channelIndex, it->second.first);
            channel.targetIndex = it->second.second;
        }

        channel.setTargetPath(gltfChannel.target_path);

        animation.channels.push_back(std::move(channel));
    }
}

void loadAnimations(std::vector<TinyAnimation>& animations, std::vector<ChannelToSkeletonMap>& ChannelToSkeletonMaps, tinygltf::Model& model, const UnorderedMap<int, std::pair<int, int>>& nodeToSkeletonAndBoneIndex) {
    animations.clear();
    ChannelToSkeletonMaps.clear();

    for (size_t animIndex = 0; animIndex < model.animations.size(); ++animIndex) {
        const tinygltf::Animation& gltfAnim = model.animations[animIndex];

        TinyAnimation animation;
        ChannelToSkeletonMap channelToSkeletonMap;
        loadAnimation(animation, channelToSkeletonMap, model, gltfAnim, nodeToSkeletonAndBoneIndex);

        animations.push_back(std::move(animation));
        ChannelToSkeletonMaps.push_back(std::move(channelToSkeletonMap));
    }
}

void loadNodes(TinyModelNew& tinyModel, const tinygltf::Model& model,
               const UnorderedMap<int, std::pair<int, int>>& nodeToSkeletonAndBoneIndex)
{
    std::vector<TinyNode> nodes;

    auto pushNode = [&](TinyNode&& node) -> int {
        int index = static_cast<int>(nodes.size());
        nodes.push_back(std::move(node));
        return index;
    };

    auto parentAndChild = [&](int parentIndex, int childIndex) {
        nodes[parentIndex].children.push_back(TinyHandle(childIndex));
        nodes[childIndex].parent = TinyHandle(parentIndex);
    };

    // Root node (index 0)
    TinyNode rootNode;
    rootNode.name = "FunnyRoot";
    pushNode(std::move(rootNode));

    const auto& submeshesMats = tinyModel.submeshesMats;

    // Skeleton parent nodes
    UnorderedMap<int, int> skeletonToModelNodeIndex;
    for (size_t skelIdx = 0; skelIdx < tinyModel.skeletons.size(); ++skelIdx) {
        TinyNode skeleNode;
        skeleNode.name = "Skeleton_" + std::to_string(skelIdx);

        TinyNode::Skeleton skel3D;
        skel3D.skeleRegistry = TinyHandle(skelIdx);

        skeleNode.add<TinyNode::Skeleton>(std::move(skel3D));

        int modelNodeIndex = pushNode(std::move(skeleNode));
        skeletonToModelNodeIndex[(int)skelIdx] = modelNodeIndex;

        parentAndChild(0, modelNodeIndex); // Child of root
    }

    // Mapping: glTF node index -> TinyNode3D index (or -1 if it's a joint)
    std::vector<int> localToGlobal(model.nodes.size(), -1);

    // First pass: reserve only for non-joints
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        if (nodeToSkeletonAndBoneIndex.find((int)i) != nodeToSkeletonAndBoneIndex.end()) {
            // It's a joint node -> skip
            localToGlobal[i] = -1;
            continue;
        }

        TinyNode placeholder;
        placeholder.name = model.nodes[i].name.empty() ? "Node" : model.nodes[i].name;

        int globalIdx = pushNode(std::move(placeholder));
        localToGlobal[i] = globalIdx;
    }

    // Second pass: fill in non-joint nodes
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        int globalIdx = localToGlobal[i];
        if (globalIdx < 0) continue; // skip joints

        const tinygltf::Node& gltfNode = model.nodes[i];
        TinyNode& target = nodes[globalIdx];

        // Transform
        glm::mat4 matrix(1.0f);
        if (!gltfNode.matrix.empty()) {
            matrix = glm::make_mat4(gltfNode.matrix.data());
        } else {
            glm::vec3 translation(0.0f);
            glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
            glm::vec3 scale(1.0f);

            if (!gltfNode.translation.empty())
                translation = glm::make_vec3(gltfNode.translation.data());
            if (!gltfNode.rotation.empty())
                rotation = glm::quat(
                    gltfNode.rotation[3],
                    gltfNode.rotation[0],
                    gltfNode.rotation[1],
                    gltfNode.rotation[2]);
            if (!gltfNode.scale.empty())
                scale = glm::make_vec3(gltfNode.scale.data());

            matrix = glm::translate(glm::mat4(1.0f), translation) *
                     glm::mat4_cast(rotation) *
                     glm::scale(glm::mat4(1.0f), scale);
        }

        // Set transform in base class
        target.transform = matrix;

        if (gltfNode.mesh >= 0) {
            TinyNode::MeshRender meshData;
            meshData.mesh = TinyHandle(gltfNode.mesh);

            bool hasValidMaterials=(gltfNode.mesh >= 0 &&
                                    gltfNode.mesh < (int)submeshesMats.size());
            meshData.submeshMats = hasValidMaterials ? submeshesMats[gltfNode.mesh] : std::vector<TinyHandle>();

            int skeletonIndex = gltfNode.skin;
            auto it = skeletonToModelNodeIndex.find(skeletonIndex);
            if (it != skeletonToModelNodeIndex.end()) {
                meshData.skeleNode = TinyHandle(it->second);
            }

            target.add<TinyNode::MeshRender>(std::move(meshData));
        } else {
            // Future logic here
        }
    }

    // Third pass: parent-child wiring (skip joints)
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        int parentGlobal = localToGlobal[i];
        if (parentGlobal < 0) continue; // skip joint as parent

        const tinygltf::Node& gltfNode = model.nodes[i];
        for (int childLocal : gltfNode.children) {
            int childGlobal = localToGlobal[childLocal];
            if (childGlobal < 0) continue; // skip joint as child
            parentAndChild(parentGlobal, childGlobal);
        }
    }

    // Attach scene roots to FunnyRoot
    if (!model.scenes.empty()) {
        const tinygltf::Scene& scene = model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0];
        for (int rootLocal : scene.nodes) {
            int rootGlobal = localToGlobal[rootLocal];
            if (rootGlobal >= 0 && !nodes[rootGlobal].parent.isValid()) {
                parentAndChild(0, rootGlobal);
            }
        }
    }

    tinyModel.nodes = std::move(nodes);
}




// Helper to print vector<TinyHandle> nicely
std::string formatVector(const std::vector<TinyHandle>& vec) {
    std::string result = "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        result += std::to_string(vec[i].index);
        if (i + 1 < vec.size()) result += ", ";
    }
    result += "]";
    return result;
}

constexpr const char* RED   = "\033[31m";
constexpr const char* WHITE = "\033[0m";

// Recursive hierarchy printer
void printNodeHierarchy(const std::vector<TinyNode>& nodes, int nodeIndex, int depth = 0) {
    using NType = TinyNode::Types;

    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) return;

    const TinyNode& node = nodes[nodeIndex];
    std::string indent(depth * 2, ' ');

    // Print node name in RED
    std::cout << indent << RED << node.name << WHITE;

    // Print the translation part of the transform
    glm::vec3 translation = glm::vec3(node.transform[3]);
    std::cout << " (Pos: " << translation.x << ", " << translation.y << ", " << translation.z << ")";

    // Append extra info
    // if (node.isType(NType::MeshRender)) {
    //     const auto& mesh = std::get<TinyNode3D::MeshRender>(node.data);
    //     std::cout << " -> MeshID=" << mesh.mesh.index
    //               << ", MaterialIDs=" << formatVector(mesh.submeshMats)
    //               << ", SkeNodeID=" << mesh.skeleNode.index;
    // if (node.isType(NType::Skeleton)) {
    //     const auto& skel = std::get<TinyNode3D::Skeleton>(node.data);
    //     std::cout << " -> SkeRegID=" << skel.skeleRegistry.index;
    // }

    std::cout << "\n";

    // Recurse into children
    for (const TinyHandle& child : node.children) {
        printNodeHierarchy(nodes, child.index, depth + 1);
    }
}

// Wrapper
void printHierarchy(const std::vector<TinyNode>& nodes) {
    if (nodes.empty()) {
        std::cout << "[Empty node list]\n";
        return;
    }
    printNodeHierarchy(nodes, 0, 0); // Root at index 0
}



TinyModelNew TinyLoader::loadModelFromGLTFNew(const std::string& filePath, bool forceStatic) {
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


    TinyModelNew result;
    if (!ok || model.meshes.empty()) return result;

    loadTextures(result.textures, model);
    loadMaterials(result.materials, model, result.textures);

    UnorderedMap<int, std::pair<int, int>> nodeToSkeletonAndBoneIndex;
    if (!forceStatic) loadSkeletons(result.skeletons, nodeToSkeletonAndBoneIndex, model);

    bool hasRigging = !forceStatic && !result.skeletons.empty();
    loadMeshes(result.meshes, result.submeshesMats, model, !hasRigging);

    loadNodes(result, model, nodeToSkeletonAndBoneIndex);

    printHierarchy(result.nodes);

    return result;
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

    loadTextures(result.textures, model);
    loadMaterials(result.materials, model, result.textures);

    UnorderedMap<int, std::pair<int, int>> nodeToSkeletonAndBoneIndex; // Node index -> {skeleton index, it's bone index}
    std::vector<TinySkeleton> skeletons;

    bool hasRigging = !forceStatic && !model.skins.empty();
    if (hasRigging) loadSkeletons(skeletons, nodeToSkeletonAndBoneIndex, model);

    hasRigging &= !skeletons.empty();
    loadMeshCombined(result.mesh, result.meshMaterials, model, !hasRigging);

    // For the time being we will only be using the first skeleton
    if (hasRigging) result.skeleton = skeletons[0];

    std::vector<ChannelToSkeletonMap> channelToSkeletonMaps; // Left: channel index, Right: skeleton index
    if (hasRigging) loadAnimations(result.animations, channelToSkeletonMaps, model, nodeToSkeletonAndBoneIndex);

    return result;
}
