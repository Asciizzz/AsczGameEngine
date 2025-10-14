#pragma once

#include "TinyExt/TinyRegistry.hpp"

#include "TinyEngine/TinyRData.hpp"

#include "TinyData/TinyCamera.hpp"
#include "TinyEngine/TinyGlobal.hpp"

struct TinyNodeRT {
    // === Copied from TinyNode ===
    std::string name = "RuntimeNode";
    TinyNode::Scope scope = TinyNode::Scope::Global; // Runtime nodes are always global scope
    uint32_t types = TinyNode::toMask(TinyNode::Types::Node);
    
    // Transform from original TinyNode (renamed from transformOverride)
    glm::mat4 localTransform = glm::mat4(1.0f);
    
    // === Runtime-specific data ===
    TinyHandle parentHandle;                       // Runtime parent handle (invalid handle = no parent)
    std::vector<TinyHandle> childrenHandles;      // Runtime children handles
    
    bool isDirty = true;
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

    void addChild(TinyHandle childHandle, TinyPool<TinyNodeRT>& rtNodesPool);
};

class TinyProject {
public:
    TinyProject(const TinyVK::Device* deviceVK);

    // Delete copy
    TinyProject(const TinyProject&) = delete;
    TinyProject& operator=(const TinyProject&) = delete;
    // No move semantics, where tf would you even want to move it to?

    // Return the scene handle in the registry
    TinyHandle addSceneFromModel(const TinyModel& model); // Returns scene handle - much simpler now!

    TinyCamera* getCamera() const { return tinyCamera.get(); }
    TinyGlobal* getGlobal() const { return tinyGlobal.get(); }
    VkDescriptorSetLayout getGlbDescSetLayout() const { return tinyGlobal->getDescLayout(); }
    VkDescriptorSet getGlbDescSet(uint32_t idx) const { return tinyGlobal->getDescSet(idx); }

// All these belows are only for testing purposes
    /**
     * Adds a scene instance to the project.
     *
     * @param sceneHandle Handle to the scene in the registry to instantiate.
     * @param rootHandle Handle to the runtime node to inherit from (optional).
     */
    void addSceneInstance(TinyHandle sceneHandle, TinyHandle rootHandle = TinyHandle(), glm::mat4 at = glm::mat4(1.0f));

    /**
     * Renders an ImGui collapsible tree view of the runtime node hierarchy
     */
    void renderNodeTreeImGui(TinyHandle nodeHandle = TinyHandle(), int depth = 0);

    /**
     * Recursively updates global transforms for all nodes starting from the specified root.
     * Always processes all nodes regardless of dirty state for guaranteed correctness.
     * 
     * @param rootNodeHandle Handle to the root node to start the recursive update from
     * @param parentGlobalTransform Global transform of the parent (identity for root nodes)
     */
    void updateGlobalTransforms(TinyHandle rootNodeHandle, const glm::mat4& parentGlobalTransform = glm::mat4(1.0f));


    /**
     * Playground function for testing - rotates root node by 90 degrees per second
     * @param dTime Delta time in seconds
     */
    void runPlayground(float dTime);

    // These are not official public methods, only for testing purposes

    const TinyPool<TinyNodeRT>& getRuntimeNodes() const { return rtNodes; }
    const std::vector<TinyHandle>& getRuntimeMeshRenderHandles() const { return rtMeshRenderHandles; }
    TinyHandle getRootNodeHandle() const { return rootNodeHandle; }
    
    // Helper to get handle by index (useful for UI that thinks in indices)
    TinyHandle getNodeHandleByIndex(uint32_t index) const { return rtNodes.getHandleByIndex(index); }

    const UniquePtr<TinyRegistry>& getRegistry() const { return registry; }

private:
    const TinyVK::Device* deviceVK;

    UniquePtr<TinyGlobal> tinyGlobal;
    UniquePtr<TinyCamera> tinyCamera;

    UniquePtr<TinyRegistry> registry;

    TinyPool<TinyNodeRT> rtNodes;
    TinyHandle rootNodeHandle; // Handle to the root node
    std::vector<TinyHandle> rtMeshRenderHandles; // Handles to rtNodes with MeshRender component
};