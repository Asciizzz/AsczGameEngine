#pragma once

#include "Az3D/VertexTypes.hpp"
#include <cstdint>
#include <vector>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "AzVulk/Buffer.hpp"
#include "AzVulk/Descriptor.hpp"

namespace Az3D {

// Forward declarations
struct BVHNode;
struct HitInfo;

// ============================================================================
// MESH STRUCTURES
// ============================================================================

// Uniform mesh structure that holds raw data only
struct Mesh {
    std::vector<uint8_t> vertexData;
    std::vector<uint32_t> indices;
    VertexLayout layout;

    Mesh() = default;

    template<typename VertexT>
    Mesh(const std::vector<VertexT>& verts, const std::vector<uint32_t>& idx) {
        layout = VertexT::getLayout();
        indices = idx;
        vertexData.resize(verts.size() * sizeof(VertexT));
        std::memcpy(vertexData.data(), verts.data(), vertexData.size());
    }

    size_t vertexCount() const { return vertexData.size() / layout.stride; }
};

// BVH structures (deprecated - to be reimplemented later)
struct BVHNode {
    glm::vec3 min;
    glm::vec3 max;

    /* Note:
    -1 children means leaf
    Leaf range is [l_leaf, r_leaf)
    */
    int l_child = -1;
    int r_child = -1;
    size_t l_leaf = 0;
    size_t r_leaf = 0;
};

struct HitInfo {
    bool hit = false;
    bool hasHit = false; // Keep both for compatibility
    size_t index = 0;
    uint32_t triangleIndex = 0; // Keep both for compatibility
    glm::vec3 prop = glm::vec3(-1.0f); // {u, v, t} (u, v are for barycentric coordinates, t is distance)
    glm::vec3 barycentric{}; // Keep both for compatibility
    float t = 1e30f; // Keep both for compatibility
    
    // Get the vertex and normal at the hit point
    glm::vec3 vrtx = glm::vec3(0.0f);
    glm::vec3 nrml = glm::vec3(0.0f);
    uint32_t materialId = 0;
};

// ============================================================================
// MATERIAL STRUCTURES
// ============================================================================

enum class TAddressMode {
    Repeat        = 0,
    ClampToEdge   = 1,
    ClampToBorder = 2
};

struct Material {
    glm::vec4 shadingParams = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); // <bool shading>, <int toonLevel>, <float normalBlend>, <float discardThreshold>
    glm::uvec4 texIndices = glm::uvec4(0, 0, 0, 0); // <albedo>, <normal>, <metallic>, <unsure>

    Material() = default;

    void setShadingParams(bool shading, int toonLevel, float normalBlend, float discardThreshold) {
        shadingParams.x = shading ? 1.0f : 0.0f;
        shadingParams.y = static_cast<float>(toonLevel);
        shadingParams.z = normalBlend;
        shadingParams.w = discardThreshold;
    }

    void setAlbedoTexture(int index, TAddressMode addressMode = TAddressMode::Repeat) {
        texIndices.x = index;
        texIndices.y = static_cast<uint32_t>(addressMode);
    }

    void setNormalTexture(int index, TAddressMode addressMode = TAddressMode::Repeat) {
        texIndices.z = index;
        texIndices.w = static_cast<uint32_t>(addressMode);
    }
};

// ============================================================================
// TEXTURE STRUCTURES
// ============================================================================

// Raw texture data (no Vulkan handles)
struct Texture {
    int width = 0;
    int height = 0;
    int channels = 0;
    uint8_t* data = nullptr;

    // Free using delete[] since we allocate with new[]
    void free() {
        if (data) {
            delete[] data;
            data = nullptr;
        }
    }
    
    // Copy constructor and assignment to prevent double deletion
    Texture(const Texture& other) = delete;
    Texture& operator=(const Texture& other) = delete;
    
    // Move constructor and assignment
    Texture(Texture&& other) noexcept 
        : width(other.width), height(other.height), channels(other.channels), data(other.data) {
        other.data = nullptr;
    }
    
    Texture& operator=(Texture&& other) noexcept {
        if (this != &other) {
            free();
            width = other.width;
            height = other.height;
            channels = other.channels;
            data = other.data;
            other.data = nullptr;
        }
        return *this;
    }
    
    Texture() = default;
    ~Texture() { free(); }
};

// Vulkan texture resource (image + view + memory only)
struct TextureVK {
    VkImage image         = VK_NULL_HANDLE;
    VkImageView view      = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

// ============================================================================
// SKELETON STRUCTURES
// ============================================================================

struct Skeleton {
    // Bone SoA
    std::vector<std::string> names;
    std::vector<int> parentIndices;
    std::vector<glm::mat4> inverseBindMatrices;
    std::vector<glm::mat4> localBindTransforms;

    std::unordered_map<std::string, int> nameToIndex;

    void debugPrintHierarchy() const;

private:
    void debugPrintRecursive(int boneIndex, int depth) const;
};

// ============================================================================
// MODEL STRUCTURE
// ============================================================================

struct Model {
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures;
    Skeleton skeleton;
    
    // Future: animations, submesh info, etc.
};

} // namespace Az3D
