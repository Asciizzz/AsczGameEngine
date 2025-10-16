#pragma once

#include "TinyData/TinyModel.hpp"

#include "TinyVK/Resource/TextureVK.hpp"
#include "TinyVK/Resource/Descriptor.hpp"
#include "TinyVK/Resource/DataBuffer.hpp"
#include <string>

struct TinyRMesh {
    std::string name; // Mesh name from source data
    TinyVK::DataBuffer vertexBuffer;
    TinyVK::DataBuffer indexBuffer;
    std::vector<TinySubmesh> submeshes;
    TinyVertexLayout vertexLayout;
    VkIndexType indexType = VK_INDEX_TYPE_UINT32;

    // Delete copy semantics, allow move semantics
    TinyRMesh() = default;
    ~TinyRMesh() = default;

    TinyRMesh(const TinyRMesh&) = delete;
    TinyRMesh& operator=(const TinyRMesh&) = delete;
    
    TinyRMesh(TinyRMesh&&) = default;
    TinyRMesh& operator=(TinyRMesh&&) = default;

    bool import(const TinyVK::Device* deviceVK, const TinyMesh& mesh);
    void setSubmeshes(const std::vector<TinySubmesh>& subs) { submeshes = subs; }

    static VkIndexType tinyToVkIndexType(TinyMesh::IndexType type);
};

struct TinyRMaterial {
    std::string name; // Material name from source data
    glm::uvec4 texIndices = glm::uvec4(0); // Albedo, Normal, Reserved, Reserved

    void setAlbTexIndex(uint32_t index) { texIndices.x = index; }
    void setNrmlTexIndex(uint32_t index) { texIndices.y = index; }
};

struct TinyRTexture {
    std::string name;
    TinyVK::TextureVK textureVK;
    bool import(const TinyVK::Device* deviceVK, const TinyTexture& texture);
};

struct TinyRSkeleton : public TinySkeleton {};

struct TinyRScene {
    std::string name;
    std::vector<TinyNode> nodes;
    
    // Helper methods for scene management
    int32_t getRootNodeIndex() const {
        // Loader creates a default root node at index 0
        return nodes.empty() ? -1 : 0;
    }
    
    bool hasNodes() const {
        return !nodes.empty();
    }
};