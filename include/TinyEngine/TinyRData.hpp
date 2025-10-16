#pragma once

#include "TinyData/TinyModel.hpp"

#include "TinyVK/Resource/TextureVK.hpp"
#include "TinyVK/Resource/Descriptor.hpp"
#include "TinyVK/Resource/DataBuffer.hpp"

#include "TinyExt/TinyPool.hpp"

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



struct TinyRNode {
    // === Copied from TinyNode ===
    std::string name = "Node";
    TinyNode::Scope scope = TinyNode::Scope::Global;
    uint32_t types = TinyNode::toMask(TinyNode::Types::Node);

    // === Runtime-specific data ===
    TinyHandle parentHandle;
    std::vector<TinyHandle> childrenHandles;

    glm::mat4 localTransform = glm::mat4(1.0f);
    glm::mat4 globalTransform = glm::mat4(1.0f);

    // === Runtime Components (enhanced versions of TinyNode components) ===

    struct MeshRender {
        static constexpr TinyNode::Types kType = TinyNode::Types::MeshRender;
        
        TinyHandle mesh;                    // Copied from scene node
        TinyHandle skeleNodeRT;             // Remapped to runtime node handle
    };

    struct BoneAttach {
        static constexpr TinyNode::Types kType = TinyNode::Types::BoneAttach;
        
        TinyHandle skeleNodeRT;             // Remapped to runtime node handle  
        TinyHandle bone;                    // Copied from scene node
    };

    struct Skeleton {
        static constexpr TinyNode::Types kType = TinyNode::Types::Skeleton;
        
        TinyHandle skeleRegistry;                        // Copied from scene node
        std::vector<glm::mat4> boneTransformsFinal;      // Runtime bone transforms
    };

private:
    std::tuple<MeshRender, BoneAttach, Skeleton> components;

    // Template magic to get component by type from tuple
    template<typename T>
    T& getComponent() {
        return std::get<T>(components);
    }

    template<typename T>
    const T& getComponent() const {
        return std::get<T>(components);
    }

public:
    // Copy constructor from TinyNode
    void copyFromSceneNode(const TinyNode& sceneNode) {
        name = sceneNode.name;
        scope = TinyNode::Scope::Global; // Runtime nodes are always global
        types = sceneNode.types;
        localTransform = sceneNode.transform;
        
        // Copy components with potential remapping done later
        if (sceneNode.hasType(TinyNode::Types::MeshRender)) {
            const auto* sceneMeshRender = sceneNode.get<TinyNode::MeshRender>();
            if (sceneMeshRender) {
                MeshRender rtMeshRender;
                rtMeshRender.mesh = sceneMeshRender->mesh;
                // skeleNodeRT will be set during scene instantiation
                add(rtMeshRender);
            }
        }
        
        if (sceneNode.hasType(TinyNode::Types::BoneAttach)) {
            const auto* sceneBoneAttach = sceneNode.get<TinyNode::BoneAttach>();
            if (sceneBoneAttach) {
                BoneAttach rtBoneAttach;
                rtBoneAttach.bone = sceneBoneAttach->bone;
                // skeleNodeRT will be set during scene instantiation
                add(rtBoneAttach);
            }
        }
        
        if (sceneNode.hasType(TinyNode::Types::Skeleton)) {
            const auto* sceneSkeleton = sceneNode.get<TinyNode::Skeleton>();
            if (sceneSkeleton) {
                Skeleton rtSkeleton;
                rtSkeleton.skeleRegistry = sceneSkeleton->skeleRegistry;
                // boneTransformsFinal will be initialized based on skeleton data
                add(rtSkeleton);
            }
        }
    }

    // Component management functions
    bool hasType(TinyNode::Types componentType) const {
        return (types & TinyNode::toMask(componentType)) != 0;
    }

    void setType(TinyNode::Types componentType, bool state) {
        if (state) types |= TinyNode::toMask(componentType);
        else       types &= ~TinyNode::toMask(componentType);
    }

    // Completely generic template functions - no knowledge of specific components!
    template<typename T>
    bool hasComponent() const {
        return hasType(T::kType);
    }

    template<typename T>
    void add(const T& componentData) {
        setType(T::kType, true);
        getComponent<T>() = componentData;
    }

    template<typename T>
    void remove() {
        setType(T::kType, false);
    }

    template<typename T>
    T* get() {
        return hasComponent<T>() ? &getComponent<T>() : nullptr;
    }

    template<typename T>
    const T* get() const {
        return hasComponent<T>() ? &getComponent<T>() : nullptr;
    }
};


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