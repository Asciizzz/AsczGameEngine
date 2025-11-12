#include "tinyEngine/tinyLoader.hpp"
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
bool LoadImageData(tinygltf::Image* image, const int image_indx, std::string* err,
                   std::string* warn, int req_width, int req_height,
                   const unsigned char* bytes, int size, void* user_data) {
    (void)image_indx;
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

// Helper function to convert any channel count to RGBA
static std::vector<uint8_t> convertToRGBA(const uint8_t* srcData, int width, int height, int channels) {
    size_t pixelCount = width * height;
    std::vector<uint8_t> rgbaData(pixelCount * 4);
    
    if (channels == 1) {
        // Grayscale -> RGBA
        for (size_t i = 0; i < pixelCount; ++i) {
            uint8_t gray = srcData[i];
            rgbaData[i * 4 + 0] = gray;
            rgbaData[i * 4 + 1] = gray;
            rgbaData[i * 4 + 2] = gray;
            rgbaData[i * 4 + 3] = 255;
        }
    } else if (channels == 2) {
        // Grayscale + Alpha -> RGBA
        for (size_t i = 0; i < pixelCount; ++i) {
            uint8_t gray = srcData[i * 2 + 0];
            uint8_t alpha = srcData[i * 2 + 1];
            rgbaData[i * 4 + 0] = gray;
            rgbaData[i * 4 + 1] = gray;
            rgbaData[i * 4 + 2] = gray;
            rgbaData[i * 4 + 3] = alpha;
        }
    } else if (channels == 3) {
        // RGB -> RGBA
        for (size_t i = 0; i < pixelCount; ++i) {
            rgbaData[i * 4 + 0] = srcData[i * 3 + 0];
            rgbaData[i * 4 + 1] = srcData[i * 3 + 1];
            rgbaData[i * 4 + 2] = srcData[i * 3 + 2];
            rgbaData[i * 4 + 3] = 255;
        }
    } else if (channels == 4) {
        // RGBA -> RGBA (copy as-is)
        std::memcpy(rgbaData.data(), srcData, pixelCount * 4);
    }
    
    return rgbaData;
}

tinyTexture tinyLoader::loadTexture(const std::string& filePath) {
    tinyTexture texture = {};

    int width = 0, height = 0, channels = 0;

    // Load image using stbi with original channel count
    uint8_t* stbiData = stbi_load(
        filePath.c_str(), &width, &height, &channels,
        0  // Don't force channel conversion, preserve original format
    );
    
    // Check if loading failed
    if (!stbiData) { return texture; }

    // Convert to RGBA for consistency
    std::vector<uint8_t> rgbaData = convertToRGBA(stbiData, width, height, channels);
    
    // Free stbi allocated memory
    stbi_image_free(stbiData);

    // Always create texture with 4 channels (RGBA)
    return texture.create(std::move(rgbaData), width, height, 4);
}

// =================================== 3D MODELS ===================================

template<typename T>
bool validIndex(int index, const std::vector<T>& vec) {
    return index >= 0 && static_cast<size_t>(index) < vec.size();
}

// Helper for static_assert false
template<class> struct always_false : std::false_type {};
template<typename T>
bool readAccessor(const tinygltf::Model& model, int accessorIndex, std::vector<T>& out)
{
    if (accessorIndex < 0) return false;

    const auto& accessor = model.accessors[accessorIndex];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];

    const size_t compSize = [&]() {
        switch(accessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_FLOAT: return sizeof(float);
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return sizeof(uint16_t);
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: return sizeof(uint32_t);
            case TINYGLTF_COMPONENT_TYPE_BYTE: return sizeof(int8_t);
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return sizeof(uint8_t);
            case TINYGLTF_COMPONENT_TYPE_SHORT: return sizeof(int16_t);
            default: return size_t(0);
        }
    }();
    if (compSize == 0) return false;

    const size_t numComponents = [&]() {
        switch(accessor.type) {
            case TINYGLTF_TYPE_SCALAR: return 1;
            case TINYGLTF_TYPE_VEC2:   return 2;
            case TINYGLTF_TYPE_VEC3:   return 3;
            case TINYGLTF_TYPE_VEC4:   return 4;
            case TINYGLTF_TYPE_MAT2:   return 4;
            case TINYGLTF_TYPE_MAT3:   return 9;
            case TINYGLTF_TYPE_MAT4:   return 16;
            default: return 1;
        }
    }();

    const size_t stride = bufferView.byteStride ? bufferView.byteStride : compSize * numComponents;
    const unsigned char* dataPtr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

    out.resize(accessor.count);

    for (size_t i = 0; i < accessor.count; ++i)
    {
        const unsigned char* elemPtr = dataPtr + i * stride;

        if constexpr (std::is_same_v<T, float>) {
            // For scalar floats
            if (numComponents != 1 || accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) return false;
            out[i] = *reinterpret_cast<const float*>(elemPtr);
        }
        else if constexpr (std::is_same_v<T, glm::vec2> ||
                           std::is_same_v<T, glm::vec3> ||
                           std::is_same_v<T, glm::vec4>) {

            if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) return false;

            glm::vec4 v(0.f); // promote vec3→vec4 safely
            for (size_t c = 0; c < numComponents && c < 4; ++c) {
                v[c] = *reinterpret_cast<const float*>(elemPtr + c * compSize);
            }

            if constexpr (std::is_same_v<T, glm::vec2>) out[i] = glm::vec2(v);
            if constexpr (std::is_same_v<T, glm::vec3>) out[i] = glm::vec3(v);
            if constexpr (std::is_same_v<T, glm::vec4>) out[i] = v;
        }
        else if constexpr (std::is_same_v<T, glm::mat4>) {
            if (accessor.type != TINYGLTF_TYPE_MAT4 || accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
                return false;

            glm::mat4 m(1.f);
            for (size_t col = 0; col < 4; ++col) {
                for (size_t row = 0; row < 4; ++row) {
                    m[col][row] = *reinterpret_cast<const float*>(elemPtr + (col*4 + row) * compSize);
                }
            }
            out[i] = m;
        }
        else if constexpr (std::is_same_v<T, glm::uvec4>) {
            // Only support SCALAR arrays packed as 4-component vectors
            if (numComponents != 4) return false;
            glm::uvec4 v(0); // declare temp variable

            switch (accessor.componentType) {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    for (size_t c = 0; c < 4; ++c) {
                        v[c] = static_cast<unsigned int>(*(elemPtr + c * compSize));
                    }
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    for (size_t c = 0; c < 4; ++c) {
                        v[c] = static_cast<unsigned int>(*(reinterpret_cast<const uint16_t*>(elemPtr) + c));
                    }
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    for (size_t c = 0; c < 4; ++c) {
                        v[c] = *(reinterpret_cast<const uint32_t*>(elemPtr) + c);
                    }
                    break;
                default:
                    return false;
            }
            out[i] = glm::uvec4(v);
        }
        else {
            // Unsupported type
            return false;
        }
    }

    return true;
}



// Reads from a map of attributes, falls back to readAccessor
template<typename T>
bool readAccessorFromMap(const tinygltf::Model& model,
                         const std::map<std::string, int>& attributes,
                         const std::string& key,
                         std::vector<T>& out) {
    auto it = attributes.find(key);
    if (it == attributes.end()) return false;

    return readAccessor<T>(model, it->second, out);
}


std::string sanitizeAsciiz(const std::string& originalName, const std::string& key, size_t fallbackIndex) {
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

tinyModel tinyLoader::loadModel(const std::string& filePath, bool forceStatic) {
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
        return tinyModel(); // Unsupported format
    }
}







// New implemetation

void loadTextures(std::vector<tinyModel::Texture>& textures, tinygltf::Model& model) {
    textures.clear();
    textures.reserve(model.textures.size());

    for (const auto& gltfTexture : model.textures) {
        tinyTexture texture;

        // Load image data
        if (gltfTexture.source >= 0 && gltfTexture.source < static_cast<int>(model.images.size())) {
            const auto& image = model.images[gltfTexture.source];
            
            // Convert to RGBA for consistency
            std::vector<uint8_t> rgbaData = convertToRGBA(
                image.image.data(), image.width, image.height, image.component
            );
            
            // Always create texture with 4 channels (RGBA)
            texture = tinyTexture().create(std::move(rgbaData), image.width, image.height, 4);
        }
        
        // Load sampler settings (address mode)
        if (gltfTexture.sampler >= 0 && gltfTexture.sampler < static_cast<int>(model.samplers.size())) {
            const auto& sampler = model.samplers[gltfTexture.sampler];
            
            // Convert GLTF wrap modes to our AddressMode enum
            // GLTF uses the same values for both wrapS and wrapT, so we'll use wrapS
            switch (sampler.wrapS) {
                case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                    texture.setWrapMode(tinyTexture::WrapMode::ClampToEdge);
                    break;

                case TINYGLTF_TEXTURE_WRAP_REPEAT:
                case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT: // Fall back
                default:
                    texture.setWrapMode(tinyTexture::WrapMode::Repeat);
                    break;
            }
        }

        tinyModel::Texture texEntry;
        texEntry.name = sanitizeAsciiz(gltfTexture.name, "texture", textures.size());
        texEntry.texture = std::move(texture);

        textures.push_back(std::move(texEntry));
    }
}

void loadMaterials(std::vector<tinyModel::Material>& materials, tinygltf::Model& model, const std::vector<tinyModel::Texture>& textures) {
    materials.clear();
    materials.reserve(model.materials.size());

    for (size_t matIndex = 0; matIndex < model.materials.size(); matIndex++) {
        const auto& gltfMaterial = model.materials[matIndex];
        tinyModel::Material material;
        material.name = sanitizeAsciiz(gltfMaterial.name, "material", matIndex);

        // Extract base color factor (default is white if not specified)
        const auto& baseColorFactor = gltfMaterial.pbrMetallicRoughness.baseColorFactor;
        if (size_t colorSize = baseColorFactor.size(); colorSize >= 1) {
            // Handle 1, 2, 3, or 4 channel base colors
            float r = colorSize > 0 ? static_cast<float>(baseColorFactor[0]) : 1.0f;
            float g = colorSize > 1 ? static_cast<float>(baseColorFactor[1]) : r;
            float b = colorSize > 2 ? static_cast<float>(baseColorFactor[2]) : r;
            float a = colorSize > 3 ? static_cast<float>(baseColorFactor[3]) : 1.0f;

            material.baseColor = glm::vec4(r, g, b, a);
        }

        // Albedo/Base color texture
        int albedoTexIndex = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;
        if (validIndex(albedoTexIndex, textures)) {
            material.albIndx = albedoTexIndex;
        }
    
        // Normal texture
        int normalTexIndex = gltfMaterial.normalTexture.index;
        if (validIndex(normalTexIndex, textures)) {
            material.nrmlIndx = normalTexIndex;
        }

        // Metallic-Roughness texture (combined in glTF)
        // Note: In glTF, metallic is in B channel, roughness is in G channel
        int metallicRoughnessTexIndex = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
        if (validIndex(metallicRoughnessTexIndex, textures)) {
            material.metalIndx = metallicRoughnessTexIndex;
        }

        int emissiveTexIndex = gltfMaterial.emissiveTexture.index;
        if (validIndex(emissiveTexIndex, textures)) {
            material.emisIndx = emissiveTexIndex;
        }

        materials.push_back(material);
    }
}

struct PrimitiveData {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec4> tangents;
    std::vector<glm::vec4> colors;     // Vertex colors (COLOR_0)
    std::vector<glm::uvec4> boneIDs;
    std::vector<glm::vec4> weights;
    std::vector<uint32_t> indices; // Initial conversion
    int materialIndex = -1;
    size_t vrtxCount = 0;
};

template <typename T>
static bool readDeltaAccessor(const tinygltf::Model& model,
                              int accessorIdx,
                              std::vector<T>& out,
                              size_t requiredCount)
{
    // ---- 1. Invalid accessor index ----
    if (accessorIdx < 0 || static_cast<size_t>(accessorIdx) >= model.accessors.size()) {
        out.assign(requiredCount, T(0));  // fill with zero
        return false;
    }

    const auto& acc = model.accessors[accessorIdx];

    // ---- 2. Must be vec3 + float ----
    if (acc.type != TINYGLTF_TYPE_VEC3 ||
        acc.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
        out.assign(requiredCount, T(0));
        return false;
    }

    // ---- 3. Buffer view must exist ----
    if (acc.bufferView < 0 || static_cast<size_t>(acc.bufferView) >= model.bufferViews.size()) {
        out.assign(requiredCount, T(0));
        return false;
    }

    const auto& bv  = model.bufferViews[acc.bufferView];
    const auto& buf = model.buffers[bv.buffer];

    // ---- 4. Bounds check on buffer data ----
    size_t byteOffset = bv.byteOffset + acc.byteOffset;
    size_t totalBytesNeeded = acc.count * sizeof(T);
    if (byteOffset + totalBytesNeeded > buf.data.size()) {
        out.assign(requiredCount, T(0));
        return false;
    }

    // ---- 5. Stride (safe) ----
    size_t stride = acc.ByteStride(bv);
    if (stride == 0) stride = sizeof(T);

    const uint8_t* src = buf.data.data() + byteOffset;

    // ---- 6. Read only requiredCount (in case accessor is larger) ----
    out.resize(requiredCount);
    for (size_t i = 0; i < requiredCount; ++i) {
        if (i >= acc.count) {
            out[i] = T(0);
        } else {
            std::memcpy(&out[i], src + i * stride, sizeof(T));
        }
    }

    return true;
}

void loadMesh(tinyMesh& mesh,
              const tinygltf::Model& gltfModel,
              const tinygltf::Mesh& gltfMesh,
              const std::vector<tinygltf::Primitive>& primitives,
              bool hasRigging)
{
    std::vector<PrimitiveData> allPrimitiveDatas;
    VkIndexType largestIndexType = VK_INDEX_TYPE_UINT8;

    for (const auto& primitive : primitives) {
        PrimitiveData pData;

        // ---- required POSITION ------------------------------------------------
        if (!readAccessorFromMap(gltfModel, primitive.attributes, "POSITION", pData.positions))
            throw std::runtime_error("Primitive missing POSITION");
        pData.vrtxCount = pData.positions.size();

        // ---- optional attributes ------------------------------------------------
        readAccessorFromMap(gltfModel, primitive.attributes, "NORMAL",    pData.normals);
        readAccessorFromMap(gltfModel, primitive.attributes, "TANGENT",   pData.tangents);
        readAccessorFromMap(gltfModel, primitive.attributes, "TEXCOORD_0",pData.uvs);

        // Read vertex colors (COLOR_0) - store for later texture atlas generation
        readAccessorFromMap(gltfModel, primitive.attributes, "COLOR_0", pData.colors);

        if (hasRigging) {
            readAccessorFromMap(gltfModel, primitive.attributes, "JOINTS_0", pData.boneIDs);
            readAccessorFromMap(gltfModel, primitive.attributes, "WEIGHTS_0", pData.weights);
        }

        // ---- indices ------------------------------------------------------------
        if (primitive.indices >= 0) {
            const auto& acc = gltfModel.accessors[primitive.indices];
            const auto& bv  = gltfModel.bufferViews[acc.bufferView];
            const auto& buf = gltfModel.buffers[bv.buffer];
            const uint8_t* src = buf.data.data() + bv.byteOffset + acc.byteOffset;
            size_t stride = acc.ByteStride(bv);

            auto append = [&](auto dummy) {
                using T = decltype(dummy);
                for (size_t i = 0; i < acc.count; ++i) {
                    T v; std::memcpy(&v, src + i*stride, sizeof(T));
                    pData.indices.push_back(static_cast<uint32_t>(v));
                }
            };

            VkIndexType curType;
            switch (acc.componentType) {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  append(uint8_t{});  curType = VK_INDEX_TYPE_UINT8;  break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: append(uint16_t{}); curType = VK_INDEX_TYPE_UINT16; break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   append(uint32_t{}); curType = VK_INDEX_TYPE_UINT32; break;
                default: throw std::runtime_error("Unsupported index type");
            }

            // keep the *largest* type for the whole mesh
            if (curType == VK_INDEX_TYPE_UINT32) largestIndexType = VK_INDEX_TYPE_UINT32;
            else if (curType == VK_INDEX_TYPE_UINT16 && largestIndexType != VK_INDEX_TYPE_UINT32)
                largestIndexType = VK_INDEX_TYPE_UINT16;
        }

        pData.materialIndex = primitive.material;
        allPrimitiveDatas.push_back(std::move(pData));
    }

    if (allPrimitiveDatas.empty()) return; // no primitives

    std::vector<uint32_t>           allIndices;
    std::vector<tinyVertex::Rigged> allVertices;   // will be converted later

    uint32_t vtxOffset = 0, idxOffset = 0;

    for (const auto& p : allPrimitiveDatas) {
        // ---- vertices -------------------------------------------------------
        for (size_t i = 0; i < p.vrtxCount; ++i) {
            tinyVertex::Rigged v;
            v.setPos   (i < p.positions.size() ? p.positions[i] : glm::vec3(0.f));
            v.setNrml  (i < p.normals.size()   ? p.normals[i]   : glm::vec3(0.f));
            v.setUV    (i < p.uvs.size()       ? p.uvs[i]       : glm::vec2(0.f));
            v.setTang  (i < p.tangents.size()  ? p.tangents[i]  : glm::vec4(1,0,0,1));

            if (hasRigging && i < p.boneIDs.size() && i < p.weights.size()) {
                v.setBoneIDs(p.boneIDs[i]);
                v.setBoneWs (p.weights[i], true);
            }
            allVertices.push_back(v);
        }

        // ---- indices --------------------------------------------------------
        for (uint32_t idx : p.indices)
            allIndices.push_back(idx + vtxOffset);

        // ---- part -----------------------------------------------------------
        tinyMesh::Part part;
        part.indxOffset = idxOffset;
        part.indxCount  = static_cast<uint32_t>(p.indices.size());
        part.material   = (p.materialIndex >= 0) ? tinyHandle(p.materialIndex) : tinyHandle();
        mesh.addPart(part);

        vtxOffset += static_cast<uint32_t>(p.vrtxCount);
        idxOffset += static_cast<uint32_t>(p.indices.size());
    }

    switch (largestIndexType) {
        case VK_INDEX_TYPE_UINT8: {
            std::vector<uint8_t> tmp; tmp.reserve(allIndices.size());
            for (uint32_t i : allIndices) tmp.push_back(static_cast<uint8_t>(i));
            mesh.setIndxs(tmp);
        } break;
        case VK_INDEX_TYPE_UINT16: {
            std::vector<uint16_t> tmp; tmp.reserve(allIndices.size());
            for (uint32_t i : allIndices) tmp.push_back(static_cast<uint16_t>(i));
            mesh.setIndxs(tmp);
        } break;
        case VK_INDEX_TYPE_UINT32:
            mesh.setIndxs(allIndices); break;
        default: break;
    }

    if (hasRigging) mesh.setVrtxs(allVertices);
    else mesh.setVrtxs(tinyVertex::Rigged::makeStatic(allVertices));

    for (size_t primIdx = 0; primIdx < primitives.size(); ++primIdx) {
        const auto& primitive = primitives[primIdx];
        const auto& pData     = allPrimitiveDatas[primIdx];

        // each entry in primitive.targets is one morph target
        for (size_t tgtIdx = 0; tgtIdx < primitive.targets.size(); ++tgtIdx) {
            const auto& target = primitive.targets[tgtIdx];

            // ---- read delta accessors (all are optional in glTF spec) ----------------
            std::vector<glm::vec3> dPos(pData.vrtxCount, glm::vec3(0.0f));
            std::vector<glm::vec3> dNrm(pData.vrtxCount, glm::vec3(0.0f));
            std::vector<glm::vec3> dTan(pData.vrtxCount, glm::vec3(0.0f));
            
            bool hasAnyData = false;
            
            // Try to read POSITION deltas (most common)
            auto posIt = target.find("POSITION");
            if (posIt != target.end()) {
                if (readDeltaAccessor(gltfModel, posIt->second, dPos, pData.vrtxCount)) {
                    hasAnyData = true;
                }
            }
            
            // Try to read NORMAL deltas (optional)
            auto nrmIt = target.find("NORMAL");
            if (nrmIt != target.end()) {
                if (readDeltaAccessor(gltfModel, nrmIt->second, dNrm, pData.vrtxCount)) {
                    hasAnyData = true;
                }
            }
            
            // Try to read TANGENT deltas (optional)
            auto tanIt = target.find("TANGENT");
            if (tanIt != target.end()) {
                if (readDeltaAccessor(gltfModel, tanIt->second, dTan, pData.vrtxCount)) {
                    hasAnyData = true;
                }
            }
            
            // Skip only if we couldn't read ANY data at all
            if (!hasAnyData) {
                continue;
            }

            // ---- build tinyMorph array ------------------------------------------------
            std::vector<tinyMorph> deltas(pData.vrtxCount);
            for (size_t v = 0; v < pData.vrtxCount; ++v) {
                deltas[v].dPos  = dPos[v];
                deltas[v].dNrml = dNrm[v];
                // glTF tangent delta is vec3 (xyz), w is unchanged → we keep 0
                deltas[v].dTang = dTan[v];
            }

            // ---- name (optional) ----------------------------------------------------
            // glTF stores target names at the mesh level in mesh.extras["targetNames"]
            std::string name = "";
            if (tgtIdx < gltfMesh.extras.Get("targetNames").ArrayLen()) {
                name = gltfMesh.extras.Get("targetNames").Get(static_cast<int>(tgtIdx)).Get<std::string>();
            }
            
            // Fallback to default naming if no name provided
            if (name.empty()) {
                name = "MorphTarget_" + std::to_string(tgtIdx);
            }
            
            name = sanitizeAsciiz(name, "morph", tgtIdx);

            // ---- add to mesh ---------------------------------------------------------
            tinyMorphTarget mt;
            mt.name   = std::move(name);
            mt.deltas = std::move(deltas);
            mesh.addMrphTarget(mt);
        }
    }
}

void loadMeshes(std::vector<tinyModel::Mesh>& meshes, tinygltf::Model& gltfModel, bool forceStatic) {
    meshes.clear();

    for (size_t meshIndex = 0; meshIndex < gltfModel.meshes.size(); meshIndex++) {
        const tinygltf::Mesh& gltfMesh = gltfModel.meshes[meshIndex];
        tinyModel::Mesh meshEntry;
        meshEntry.name = sanitizeAsciiz(gltfMesh.name, "mesh", meshIndex);

        loadMesh(meshEntry.mesh, gltfModel, gltfMesh, gltfMesh.primitives, !forceStatic);

        meshes.push_back(std::move(meshEntry));
    }
}


// Animation target bones, leading to a complex reference layer

void loadSkeleton(tinySkeleton& skeleton, UnorderedMap<int, std::pair<int, int>>& gltfNodeToSkeletonAndBoneIndex, int skeletonIndex, const tinygltf::Model& model, const tinygltf::Skin& skin) {
    if (skin.joints.empty()) return;

    skeleton.clear();

    // Create the node-to-bone mapping
    for (int i = 0; i < skin.joints.size(); ++i) {
        int nodeIndex = skin.joints[i];

        gltfNodeToSkeletonAndBoneIndex[nodeIndex] = {skeletonIndex, i};
    }

    // Parent mapping
    UnorderedMap<int, int> nodeToParent; // Left: child node index, Right: parent node index
    for (int nodeIndx = 0; nodeIndx < model.nodes.size(); ++nodeIndx) {
        const auto& node = model.nodes[nodeIndx];

        for (int childIndx : node.children) nodeToParent[childIndx] = nodeIndx;
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

        tinyBone bone;
        std::string originalName = node.name.empty() ? "" : node.name;
        bone.name = sanitizeAsciiz(originalName, "Bone", i);
        bone.bindInverse = skeletonInverseBindMatrices[i];
        bone.bindPose = makeLocalFromNode(node);

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

void loadSkeletons(std::vector<tinyModel::Skeleton>& skeletons, UnorderedMap<int, std::pair<int, int>>& gltfNodeToSkeletonAndBoneIndex, tinygltf::Model& model) {
    skeletons.clear();
    gltfNodeToSkeletonAndBoneIndex.clear();

    for (size_t skinIndex = 0; skinIndex < model.skins.size(); ++skinIndex) {
        tinyModel::Skeleton skeletonEntry;
    
        const tinygltf::Skin& skin = model.skins[skinIndex];
        loadSkeleton(skeletonEntry.skeleton, gltfNodeToSkeletonAndBoneIndex, skinIndex, model, skin);
        skeletonEntry.name = sanitizeAsciiz(skin.name, "skeleton", skinIndex);

        skeletons.push_back(std::move(skeletonEntry));
    }
}


void loadNodes(tinyModel& tinyModel, std::vector<int>& gltfNodeToModelNode, UnorderedMap<int, int>& skeletonToModelNodeIndex, const tinygltf::Model& model, const UnorderedMap<int, std::pair<int, int>>& gltfNodeToSkeletonAndBoneIndex) {
    using tNode = tinyModel::Node;
    
    std::vector<tNode> nodes;

    auto pushNode = [&](tNode&& node) -> int {
        int index = static_cast<int>(nodes.size());
        nodes.push_back(std::move(node));
        return index;
    };

    tNode trueRoot(tinyModel.name.empty() ? "Model_Root" : tinyModel.name);
    pushNode(std::move(trueRoot));

    // Construct all root-level nodes first

    if (!tinyModel.skeletons.empty()) {
        // Add a single node to store all skeletons
        tNode skeletonRootNode("LSkeletons");
        int skeletonRootModelIndex = pushNode(std::move(skeletonRootNode));

        nodes[0].addChild(skeletonRootModelIndex);
        nodes[skeletonRootModelIndex].setParent(0);

        // Skeleton parent nodes
        skeletonToModelNodeIndex.clear();
        for (size_t skeleIndx = 0; skeleIndx < tinyModel.skeletons.size(); ++skeleIndx) {
            // We only need this skeleton for the name
            const tinyModel::Skeleton& skeleton = tinyModel.skeletons[skeleIndx];

            tNode skeleNode(skeleton.name);
            skeleNode.SKEL3D_skeleIndx = skeleIndx;

            int skeleNodeIndex = pushNode(std::move(skeleNode));
            skeletonToModelNodeIndex[(int)skeleIndx] = skeleNodeIndex;

            // Child of skeleton root
            nodes[skeletonRootModelIndex].addChild(skeleNodeIndex);
            nodes[skeleNodeIndex].setParent(skeletonRootModelIndex);

            skeletonToModelNodeIndex[(int)skeleIndx] = skeleNodeIndex;
        }
    }

    std::vector<int> gltfNodeParent(model.nodes.size(), -1);
    std::vector<bool> gltfNodeIsJoint(model.nodes.size(), false);
    gltfNodeToModelNode.resize(model.nodes.size(), -1);

    for (int i = 0; i < model.nodes.size(); ++i) {
        for (int childIndx : model.nodes[i].children) {
            gltfNodeParent[childIndx] = i;
        }

        if (gltfNodeToSkeletonAndBoneIndex.find(i) !=
            gltfNodeToSkeletonAndBoneIndex.end()) {
            gltfNodeIsJoint[i] = true;
            continue; // It's a joint node -> skip
        }

        tNode emptyNode(model.nodes[i].name.empty() ? "Node" : model.nodes[i].name);

        int modelIndx = pushNode(std::move(emptyNode));
        gltfNodeToModelNode[i] = modelIndx;
    }

    // Second pass: parent-child wiring + setting details
    for (int i = 0; i < model.nodes.size(); ++i) {
        const tinygltf::Node& nGltf = model.nodes[i];

        int nModelIndex = gltfNodeToModelNode[i];
        if (nModelIndex < 0) continue; // skip joint

        tNode& nModel = nodes[nModelIndex];

        // Establish parent-child relationship
        int parentGltfIndex = gltfNodeParent[i];
        int parentModelIndex = -1;
        if (parentGltfIndex >= 0) {
            parentModelIndex = gltfNodeToModelNode[parentGltfIndex];
            parentModelIndex = parentModelIndex < 0 ? 0 : parentModelIndex; // Fallback to true root

            nodes[parentModelIndex].addChild(nModelIndex);
            nModel.setParent(parentModelIndex);
        } else {
            nodes[0].addChild(nModelIndex);
            nModel.setParent(0);
        }

        // Start adding components shall we?

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

        nModel.TRFM3D = matrix;

        if (nGltf.mesh >= 0) {
            nModel.MESHRD_meshIndx = nGltf.mesh;

            int skeletonIndex = nGltf.skin;
            auto it = skeletonToModelNodeIndex.find(skeletonIndex);
            if (it != skeletonToModelNodeIndex.end()) {
                nModel.MESHRD_skeleNodeIndx = it->second;
            }

            // Check if the parent of this node is a joint
            auto itBone = gltfNodeToSkeletonAndBoneIndex.find(parentGltfIndex);
            if (itBone != gltfNodeToSkeletonAndBoneIndex.end()) {
                // This node is associated with a bone
                int skeleIndx = itBone->second.first;
                int boneIndx = itBone->second.second;

                auto skeleIt = skeletonToModelNodeIndex.find(skeleIndx);
                if (skeleIt != skeletonToModelNodeIndex.end()) {
                    nModel.BONE3D_skeleNodeIndx = skeleIt->second;
                    nModel.BONE3D_boneIndx = boneIndx;
                }
            }
        }
    }

    tinyModel.nodes = std::move(nodes);
}


void loadAnimations(tinyModel& tinyModel, const tinygltf::Model& model, const std::vector<int>& gltfNodeToModelNode, const UnorderedMap<int, std::pair<int, int>>& gltfNodeToSkeletonAndBoneIndex, const UnorderedMap<int, int>& skeletonToModelNodeIndex) {
    using tNode = tinyModel::Node;
    
    tinyModel.animations.clear();

    // If model has no animations, return
    if (model.animations.empty()) return;

    tNode animeNode("Anime");
    animeNode.ANIM3D_animeIndx = 0; // First animation

    tinyRT_ANIM3D tinyAnim;

    for (size_t animIndex = 0; animIndex < model.animations.size(); ++animIndex) {
        const tinygltf::Animation& gltfAnim = model.animations[animIndex];
        tinyRT_ANIM3D::Clip anime;

        anime.name = sanitizeAsciiz(gltfAnim.name, "animation", animIndex);

        // Process channels and samplers here...

        for (const auto& gltfSampler : gltfAnim.samplers) {
            tinyRT_ANIM3D::Sampler sampler;

            if (gltfSampler.input >= 0) {
                readAccessor(model, gltfSampler.input, sampler.times);
            }

            if (gltfSampler.output >= 0) {
                readAccessor(model, gltfSampler.output, sampler.values);
            }

            using SamplerInterp = tinyRT_ANIM3D::Sampler::Interp;
            if (gltfSampler.interpolation == "LINEAR") {
                sampler.interp = SamplerInterp::Linear;
            } else if (gltfSampler.interpolation == "STEP") {
                sampler.interp = SamplerInterp::Step;
            } else if (gltfSampler.interpolation == "CUBICSPLINE") {
                sampler.interp = SamplerInterp::CubicSpline;
            } else {
                sampler.interp = SamplerInterp::Linear; // Default
            }

            anime.samplers.push_back(std::move(sampler));
        }

        // No need to calc animation duration here, it's done in tinyRT_ANIM3D

        for (const auto& gltfChannel : gltfAnim.channels) {
            // Retrieve the target node
            int gltfTargetNode = gltfChannel.target_node;
    
            int modelNodeIndex = gltfNodeToModelNode[gltfTargetNode];

            // Determine the property being animated
            using AnimePath = tinyRT_ANIM3D::Channel::Path;
            const std::string& path = gltfChannel.target_path;
            
            // Special handling for morph target weights
            if (path == "weights") {
                // In glTF, weights animation outputs an array of N scalars per keyframe,
                // where N is the number of morph targets on the mesh.
                // We need to split this into separate channels - one per morph target.
                
                const auto& gltfSampler = gltfAnim.samplers[gltfChannel.sampler];
                
                // Validate sampler
                if (gltfSampler.input < 0 || gltfSampler.output < 0) continue;
                if (gltfSampler.input >= static_cast<int>(model.accessors.size())) continue;
                if (gltfSampler.output >= static_cast<int>(model.accessors.size())) continue;
                
                const auto& inputAccessor = model.accessors[gltfSampler.input];
                const auto& outputAccessor = model.accessors[gltfSampler.output];
                
                size_t numKeyframes = inputAccessor.count;
                size_t totalOutputValues = outputAccessor.count;
                
                // Calculate number of morph targets: totalValues / numKeyframes
                if (numKeyframes == 0) continue;
                size_t numMorphTargets = totalOutputValues / numKeyframes;
                if (numMorphTargets == 0) continue;
                
                // Read all the weight values
                std::vector<float> allWeights;
                if (!readAccessor(model, gltfSampler.output, allWeights)) continue;
                
                // Read time values
                std::vector<float> times;
                if (!readAccessor(model, gltfSampler.input, times)) continue;
                
                // Create one channel per morph target
                for (size_t morphIdx = 0; morphIdx < numMorphTargets; ++morphIdx) {
                    tinyRT_ANIM3D::Channel morphChannel;
                    morphChannel.path = AnimePath::W;
                    morphChannel.target = tinyRT_ANIM3D::Channel::Target::Morph;
                    morphChannel.node = tinyHandle(modelNodeIndex);
                    morphChannel.index = static_cast<uint32_t>(morphIdx);
                    
                    // Create a dedicated sampler for this morph target
                    tinyRT_ANIM3D::Sampler morphSampler;
                    morphSampler.times = times;
                    
                    // Extract values for this specific morph target
                    morphSampler.values.resize(numKeyframes);
                    for (size_t keyIdx = 0; keyIdx < numKeyframes; ++keyIdx) {
                        size_t weightIdx = keyIdx * numMorphTargets + morphIdx;
                        float weight = (weightIdx < allWeights.size()) ? allWeights[weightIdx] : 0.0f;
                        // Store weight in x component of vec4
                        morphSampler.values[keyIdx] = glm::vec4(weight, 0.0f, 0.0f, 0.0f);
                    }
                    
                    // Set interpolation mode
                    using SamplerInterp = tinyRT_ANIM3D::Sampler::Interp;
                    if (gltfSampler.interpolation == "LINEAR") {
                        morphSampler.interp = SamplerInterp::Linear;
                    } else if (gltfSampler.interpolation == "STEP") {
                        morphSampler.interp = SamplerInterp::Step;
                    } else if (gltfSampler.interpolation == "CUBICSPLINE") {
                        morphSampler.interp = SamplerInterp::CubicSpline;
                        // For cubic spline, we need to handle triplets [inTangent, value, outTangent]
                        // Recalculate values
                        size_t numTriplets = numKeyframes;
                        morphSampler.values.resize(numTriplets * 3);
                        for (size_t tripletIdx = 0; tripletIdx < numTriplets; ++tripletIdx) {
                            for (size_t componentIdx = 0; componentIdx < 3; ++componentIdx) { // inTangent, value, outTangent
                                size_t weightIdx = (tripletIdx * 3 + componentIdx) * numMorphTargets + morphIdx;
                                float weight = (weightIdx < allWeights.size()) ? allWeights[weightIdx] : 0.0f;
                                morphSampler.values[tripletIdx * 3 + componentIdx] = glm::vec4(weight, 0.0f, 0.0f, 0.0f);
                            }
                        }
                    } else {
                        morphSampler.interp = SamplerInterp::Linear;
                    }
                    
                    // Add the sampler and set the channel to reference it
                    morphChannel.sampler = static_cast<uint32_t>(anime.samplers.size());
                    anime.samplers.push_back(std::move(morphSampler));
                    anime.channels.push_back(std::move(morphChannel));
                }
                
                continue; // Done with this weights channel
            }
            
            // Handle non-morph channels
            tinyRT_ANIM3D::Channel channel;
            channel.sampler = gltfChannel.sampler;

            // Check if it's a joint node
            auto jointIt = gltfNodeToSkeletonAndBoneIndex.find(gltfTargetNode);
            bool isJoint = (jointIt != gltfNodeToSkeletonAndBoneIndex.end());

            if (isJoint) {
                int skeletonIndex = jointIt->second.first;
                int boneIndex = jointIt->second.second;

                auto skeleNodeIt = skeletonToModelNodeIndex.find(skeletonIndex);
                if (skeleNodeIt != skeletonToModelNodeIndex.end()) {
                    int skeleNodeModelIndex = skeleNodeIt->second;

                    channel.target = tinyRT_ANIM3D::Channel::Target::Bone;
                    channel.node = tinyHandle(skeleNodeModelIndex);
                    channel.index = boneIndex;
                }
            } else {
                channel.node = tinyHandle(modelNodeIndex);
            }

            // Set path for non-morph animations
            if (path == "translation")   channel.path = AnimePath::T;
            else if (path == "rotation") channel.path = AnimePath::R;
            else if (path == "scale")    channel.path = AnimePath::S;
            else continue; // Unsupported path

            anime.channels.push_back(std::move(channel));
        }

        tinyAnim.add(std::move(anime));
    }
    tinyModel.animations.push_back(std::move(tinyAnim));

    int animeNodeIndex = static_cast<int>(tinyModel.nodes.size());
    tinyModel.nodes.push_back(std::move(animeNode));

    tinyModel.nodes[0].addChild(animeNodeIndex);
    tinyModel.nodes[animeNodeIndex].setParent(0);
}


tinyModel tinyLoader::loadModelFromGLTF(const std::string& filePath, bool forceStatic) {
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


    tinyModel result;

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
    UnorderedMap<int, int> skeletonToModelNodeIndex;
    loadNodes(result, gltfNodeToModelNode, skeletonToModelNodeIndex, model, gltfNodeToSkeletonAndBoneIndex);

    loadAnimations(result, model, gltfNodeToModelNode, gltfNodeToSkeletonAndBoneIndex, skeletonToModelNodeIndex);

    return result;
}








// OBJ loader implementation using tiny_obj_loader
tinyModel tinyLoader::loadModelFromOBJ(const std::string& filePath) {
    tinyModel result;
    
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

    using Material = tinyModel::Material;

    // Convert OBJ materials to tinyMaterials
    result.materials.reserve(objMaterials.size());
    for (size_t matIndex = 0; matIndex < objMaterials.size(); matIndex++) {
        const auto& objMat = objMaterials[matIndex];
        Material material;
        material.name = sanitizeAsciiz(objMat.name, "material", matIndex);

        // Extract Kd (diffuse color) and set as base color
        // OBJ stores diffuse color in the 'diffuse' array [R, G, B]
        material.baseColor = glm::vec4(
            static_cast<float>(objMat.diffuse[0]),
            static_cast<float>(objMat.diffuse[1]),
            static_cast<float>(objMat.diffuse[2]),
            1.0f  // Alpha is always 1.0 for OBJ materials
        );

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

            
            tinyModel::Texture texEntry;

            texEntry.texture = loadTexture(texturePath);
            if (texEntry.texture.valid()) {
                // Extract just the filename for the texture name
                std::string textureName = objMat.diffuse_texname;
                size_t lastSlash = textureName.find_last_of("/\\");
                if (lastSlash != std::string::npos) {
                    textureName = textureName.substr(lastSlash + 1);
                }
                texEntry.name = sanitizeAsciiz(textureName, "texture", result.textures.size());

                result.textures.push_back(std::move(texEntry));
                material.albIndx = static_cast<int>(result.textures.size() - 1);
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
        tinyMesh mesh;

        std::vector<tinyVertex::Static> vertices;
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
            for (size_t faceIndx = 0; faceIndx < objMesh.num_face_vertices.size(); faceIndx++) {
                int faceMaterialId = objMesh.material_ids.empty() ? -1 : objMesh.material_ids[faceIndx];
                
                // Skip faces that don't belong to this material
                if (faceMaterialId != materialId) {
                    faceVertexIndex += objMesh.num_face_vertices[faceIndx];
                    continue;
                }

                unsigned int faceVertexCount = objMesh.num_face_vertices[faceIndx];
                
                // Triangulate face if necessary (convert quads+ to triangles)
                std::vector<uint32_t> faceIndices;
                
                for (unsigned int v = 0; v < faceVertexCount; v++) {
                    tinyobj::index_t indx = objMesh.indices[faceVertexIndex + v];
                    
                    // Create vertex key for deduplication
                    std::tuple<int, int, int> vertexKey = std::make_tuple(indx.vertex_index, indx.normal_index, indx.texcoord_index);
                    
                    uint32_t vertexIndex;
                    auto it = vertexMap.find(vertexKey);
                    if (it != vertexMap.end()) {
                        // Reuse existing vertex
                        vertexIndex = it->second;
                    } else {
                        // Create new vertex
                        tinyVertex::Static vertex;
                        
                        // Position
                        if (indx.vertex_index >= 0) {
                            vertex.setPos(glm::vec3(
                                attrib.vertices[3 * indx.vertex_index + 0],
                                attrib.vertices[3 * indx.vertex_index + 1],
                                attrib.vertices[3 * indx.vertex_index + 2]
                            ));
                        }
                        
                        // Normal
                        if (indx.normal_index >= 0) {
                            vertex.setNrml(glm::vec3(
                                attrib.normals[3 * indx.normal_index + 0],
                                attrib.normals[3 * indx.normal_index + 1],
                                attrib.normals[3 * indx.normal_index + 2]
                            ));
                        }
                        
                        // Texture coordinates
                        if (indx.texcoord_index >= 0) {
                            vertex.setUV(glm::vec2(
                                attrib.texcoords[2 * indx.texcoord_index + 0],
                                1.0f - attrib.texcoords[2 * indx.texcoord_index + 1] // Flip V coordinate
                            ));
                        }
                        
                        // Set default tangent (will be computed later if needed)
                        vertex.setTang(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
                        
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
        mesh.setVrtxs(vertices);
        mesh.setIndxs(indices);

        tinyMesh::Part mPart;
        mPart.indxOffset = 0;
        mPart.indxCount = static_cast<uint32_t>(indices.size());
        mPart.material = (materialId >= 0) ? tinyHandle(materialId) : tinyHandle();
        mesh.addPart(mPart);

        tinyModel::Mesh meshEntry;
        meshEntry.mesh = std::move(mesh);
        meshEntry.name = materialId >= 0 ? result.materials[materialId].name : "Mesh";

        result.meshes.push_back(std::move(meshEntry));
    }

    // Create scene hierarchy: Root node + one child node per mesh
    result.nodes.clear();
    result.nodes.reserve(1 + result.meshes.size());


    using tNode = tinyModel::Node;

    tNode rootNode(result.name.empty() ? "OBJ_Root" : result.name);
    result.nodes.push_back(std::move(rootNode));

    // Child nodes for each mesh (representing each material group)
    for (size_t meshIndex = 0; meshIndex < result.meshes.size(); meshIndex++) {
        tNode meshNode(result.meshes[meshIndex].name);
        meshNode.MESHRD_meshIndx = static_cast<int>(meshIndex);

        result.nodes.push_back(std::move(meshNode));
        int nodeIndex = static_cast<int>(result.nodes.size() - 1);

        result.nodes[nodeIndex].setParent(0); // Parent is root node
        result.nodes[0].addChild(nodeIndex);
    }

    return result;
}
