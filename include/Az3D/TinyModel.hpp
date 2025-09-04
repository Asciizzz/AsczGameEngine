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
// MESH STRUCTURES
// ============================================================================

// Uniform mesh structure that holds raw data only
struct TinySubmesh {
    std::vector<uint8_t> vertexData;
    std::vector<uint32_t> indices;
    int matIndex = -1;

    VertexLayout layout;

    TinySubmesh() = default;

    template<typename VertexT>
    TinySubmesh(const std::vector<VertexT>& verts, const std::vector<uint32_t>& idx) {
        create(verts, idx);
    }

    template<typename VertexT>
    void create(const std::vector<VertexT>& verts, const std::vector<uint32_t>& idx) {
        layout = VertexT::getLayout();
        indices = idx;
        vertexData.resize(verts.size() * sizeof(VertexT));
        std::memcpy(vertexData.data(), verts.data(), vertexData.size());
    }

    size_t vertexCount() const { return vertexData.size() / layout.stride; }
};

// ============================================================================
// MATERIAL STRUCTURES
// ============================================================================

enum class TAddressMode {
    Repeat        = 0,
    ClampToEdge   = 1,
    ClampToBorder = 2
};

struct TinyMaterial {
    bool shading = true;
    int toonLevel = 0;

    // Some debug values
    float normalBlend = 0.0f;
    float discardThreshold = 0.0f;

    int albTexture = -1;
    int nrmlTexture = -1;

    TAddressMode addressMode = TAddressMode::Repeat;

    TinyMaterial() = default;
};

// ============================================================================
// TEXTURE STRUCTURES
// ============================================================================

// Raw texture data (no Vulkan handles)
struct TinyTexture {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<uint8_t> data;

    TinyTexture() = default;
    
    // Copy constructor and assignment are now allowed with vector
    TinyTexture(const TinyTexture& other) = default;
    TinyTexture& operator=(const TinyTexture& other) = default;

    // Move constructor and assignment
    TinyTexture(TinyTexture&& other) noexcept = default;
    TinyTexture& operator=(TinyTexture&& other) noexcept = default;
    
    ~TinyTexture() = default;
};

// ============================================================================
// SKELETON STRUCTURES
// ============================================================================

struct TinySkeleton {
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

struct TinyModel {
    std::vector<TinySubmesh> meshes;
    std::vector<TinyMaterial> materials;
    std::vector<TinyTexture> textures;
    TinySkeleton skeleton;

    // Debug function to print model information
    void printDebug() const;

    // Future: animations, submesh info, etc.
};

} // namespace Az3D
