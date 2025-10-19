#pragma once

#include "TinyData/TinyModel.hpp"

#include "TinyVK/Resource/TextureVK.hpp"
#include "TinyVK/Resource/Descriptor.hpp"
#include "TinyVK/Resource/DataBuffer.hpp"

#include "TinyExt/TinyPool.hpp"

#include <string>

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

struct TinyRScene {
    std::string name;
    TinyPool<TinyNode> nodes;
    TinyHandle rootHandle;

    TinyRScene() = default;

    void updateGlbTransform(TinyHandle nodeHandle = TinyHandle(), const glm::mat4& parentGlobalTransform = glm::mat4(1.0f));

    TinyHandle addRoot(const std::string& nodeName = "Root");
    TinyHandle addNode(const std::string& nodeName = "New Node", TinyHandle parentHandle = TinyHandle());
    TinyHandle addNode(const TinyNode& nodeData, TinyHandle parentHandle = TinyHandle());

    void addScene(const TinyRScene& scene, TinyHandle parentHandle = TinyHandle());
    bool removeNode(TinyHandle nodeHandle, bool recursive = true);
    bool flattenNode(TinyHandle nodeHandle);
    bool reparentNode(TinyHandle nodeHandle, TinyHandle newParentHandle);
    
    TinyNode* getNode(TinyHandle nodeHandle);
    const TinyNode* getNode(TinyHandle nodeHandle) const;
    bool renameNode(TinyHandle nodeHandle, const std::string& newName);
};
