#pragma once

#include "TinyData/TinyModel.hpp"

#include "TinyVK/Resource/TextureVK.hpp"
#include "TinyVK/Resource/Descriptor.hpp"
#include "TinyVK/Resource/DataBuffer.hpp"

#include "TinyExt/TinyPool.hpp"

#include <string>

struct TinyRMesh : public TinyMesh {
    // Runtime Vulkan resources
    TinyVK::DataBuffer vertexBuffer;
    TinyVK::DataBuffer indexBuffer;
    VkIndexType vkIndexType = VK_INDEX_TYPE_UINT16;

    // Default constructor for internal pool use only - creates invalid state
    TinyRMesh() = default;
    explicit TinyRMesh(const TinyMesh& mesh)
        : TinyMesh(mesh), vkIndexType(tinyToVkIndexType(indexType)) {}

    TinyRMesh(const TinyRMesh&) = delete;
    TinyRMesh& operator=(const TinyRMesh&) = delete;
    
    TinyRMesh(TinyRMesh&&) = default;
    TinyRMesh& operator=(TinyRMesh&&) = default;

    // Set mesh data after default construction
    void set(const TinyMesh& mesh) {
        static_cast<TinyMesh&>(*this) = mesh; // Copy base class data
        vkIndexType = tinyToVkIndexType(indexType);
    }

    // Check if this mesh has valid data
    bool isValid() const {
        return !vertexData.empty() && !indexData.empty();
    }

    // Create Vulkan buffers from the mesh data
    bool create(const TinyVK::Device* deviceVK);

    static VkIndexType tinyToVkIndexType(TinyMesh::IndexType type);
};

struct TinyRMaterial {
    std::string name; // Material name from source data
    glm::uvec4 texIndices = glm::uvec4(0); // Albedo, Normal, Reserved, Reserved (remapped registry indices)

    // Constructor from TinyMaterial - converts local texture indices to registry indices
    TinyRMaterial() = default;
    TinyRMaterial(const TinyMaterial& material) : name(material.name) {
        // Note: texture indices will be remapped during scene loading
        // material.localAlbTexture and material.localNrmlTexture are converted to registry indices
    }

    void setAlbTexIndex(uint32_t index) { texIndices.x = index; }
    void setNrmlTexIndex(uint32_t index) { texIndices.y = index; }
};

struct TinyRTexture : public TinyTexture {
    // Runtime Vulkan resources
    TinyVK::TextureVK textureVK;

    // Default constructor for internal pool use only - creates invalid state
    TinyRTexture() = default;
    explicit TinyRTexture(const TinyTexture& texture) : TinyTexture(texture) {}

    void set(const TinyTexture& texture) {
        TinyTexture::operator=(texture);
    }
    
    TinyRTexture(const TinyRTexture&) = delete;
    TinyRTexture& operator=(const TinyRTexture&) = delete;

    TinyRTexture(TinyRTexture&&) = default;
    TinyRTexture& operator=(TinyRTexture&&) = default;

    // Check if this texture has valid data
    bool isValid() const {
        return !data.empty() && width > 0 && height > 0;
    }

    // Create Vulkan texture from the texture data
    bool create(const TinyVK::Device* deviceVK);
};

struct TinyRSkeleton : public TinySkeleton {
    // Additional runtime data can be added here if needed
    TinyRSkeleton() = default;
    explicit TinyRSkeleton(const TinySkeleton& skeleton) : TinySkeleton(skeleton) {}

    void set(const TinySkeleton& skeleton) {
        TinySkeleton::operator=(skeleton);
    }
    
    TinyRSkeleton(const TinyRSkeleton&) = delete;
    TinyRSkeleton& operator=(const TinyRSkeleton&) = delete;

    TinyRSkeleton(TinyRSkeleton&&) = default;
    TinyRSkeleton& operator=(TinyRSkeleton&&) = default;
};

using TinyRNode = TinyNode; // Alias for name consistency


struct TinyRScene {
    std::string name;
    TinyPool<TinyRNode> nodes;
    TinyHandle rootHandle;

    TinyRScene() = default;

    void updateGlbTransform(TinyHandle nodeHandle = TinyHandle(), const glm::mat4& parentGlobalTransform = glm::mat4(1.0f));

    TinyHandle addRoot(const std::string& nodeName = "Root");
    TinyHandle addNode(const std::string& nodeName = "New Node", TinyHandle parentHandle = TinyHandle());
    TinyHandle addNode(const TinyRNode& nodeData, TinyHandle parentHandle = TinyHandle());

    void addScene(const TinyRScene& scene, TinyHandle parentHandle = TinyHandle());
    bool removeNode(TinyHandle nodeHandle, bool recursive = true);
    bool flattenNode(TinyHandle nodeHandle);
    bool reparentNode(TinyHandle nodeHandle, TinyHandle newParentHandle);
    
    TinyRNode* getNode(TinyHandle nodeHandle);
    const TinyRNode* getNode(TinyHandle nodeHandle) const;
    bool renameNode(TinyHandle nodeHandle, const std::string& newName);
};
