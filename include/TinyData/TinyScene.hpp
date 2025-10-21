#pragma once

#include "TinyData/TinyNode.hpp"
#include "TinyExt/TinyRegistry.hpp"

struct TinyScene {
    std::string name;

    TinyScene(const std::string& sceneName = "New Scene") : name(sceneName) {}
    void setFsRegistry(const TinyRegistry& registry) { fsRegistry = &registry; }

    TinyScene(const TinyScene&) = delete;
    TinyScene& operator=(const TinyScene&) = delete;

    TinyScene(TinyScene&&) = default;
    TinyScene& operator=(TinyScene&&) = default;

    // --------- Root management ---------

    TinyHandle addRoot(const std::string& nodeName = "Root");
    void setRoot(TinyHandle handle) { rootHandle_ = handle; }
    TinyHandle rootHandle() const { return rootHandle_; }

    // --------- Node management ---------

    TinyHandle addNode(const std::string& nodeName = "New Node", TinyHandle parentHandle = TinyHandle());
    TinyHandle addNode(const TinyNode& nodeData, TinyHandle parentHandle = TinyHandle());
    TinyHandle addNodeRaw(const TinyNode& nodeData);

    bool removeNode(TinyHandle nodeHandle, bool recursive = true);
    bool flattenNode(TinyHandle nodeHandle);
    bool reparentNode(TinyHandle nodeHandle, TinyHandle newParentHandle);
    bool renameNode(TinyHandle nodeHandle, const std::string& newName);

    TinyNode* node(TinyHandle nodeHandle);
    const TinyNode* node(TinyHandle nodeHandle) const;
    uint32_t nodeCount() const;
    const std::vector<TinyNode>& nodeView() const;
    bool nodeValid(TinyHandle nodeHandle) const;
    bool nodeOccupied(uint32_t index) const;
    
    void addScene(TinyHandle sceneHandle, TinyHandle parentHandle = TinyHandle());
    void updateGlbTransform(TinyHandle nodeHandle = TinyHandle(), const glm::mat4& parentGlobalTransform = glm::mat4(1.0f));

    // -------- Component management --------- 

    template<typename T>
    void nodeAddComp(TinyHandle nodeHandle, const T& componentData = T()) {
        TinyNode* node = nodes.get(nodeHandle);
        if (!node) return;

        node->add<T>(componentData);
        T* compPtr = node->get<T>();
        if (!compPtr) return;

        if constexpr (std::is_same_v<T, TinyNode::Skeleton>) {
            // Add new TinySkeletonRT to runtime registry (for the time being put TinyNode as placeholder)
            compPtr->rtSkeleHandle = addRT<TinyNode>(TinyNode());
        }

        // Other component-specific logic can go here
    }

    template<typename T>
    void nodeRemoveComp(TinyHandle nodeHandle) {
        TinyNode* node = nodes.get(nodeHandle);
        if (!node) return;

        // Resolve component removal logic

        if constexpr (std::is_same_v<T, TinyNode::Skeleton>) {
            TinyNode::Skeleton* skelComp = node->get<TinyNode::Skeleton>();
            if (skelComp && skelComp->rtSkeleHandle.valid()) {
                // Future implementation: Remove TinySkeletonRT
                // rtRegistry.remove<TinySkeletonRT>(skelComp->rtSkeleHandle);
            }
        }

        node->remove<T>();
    }

    template<typename T>
    T* nodeComp(TinyHandle nodeHandle) {
        TinyNode* node = nodes.get(nodeHandle);
        if (!node) return nullptr;

        return node->get<T>();
    }

    template<typename T>
    const T* nodeComp(TinyHandle nodeHandle) const {
        const TinyNode* node = nodes.get(nodeHandle);
        if (!node) return nullptr;

        return node->get<T>();
    }

private: // Immutable data
    TinyPool<TinyNode> nodes;
    TinyHandle rootHandle_{};
    TinyRegistry rtRegistry; // Runtime registry data for node
    const TinyRegistry* fsRegistry = nullptr; // Pointer to filesystem registry for resource lookups

    // --------- Runtime registry access ----------

    template<typename T>
    TypeHandle addRT(T& data) {
        return rtRegistry.add<T>(std::move(data));
    }

    template<typename T>
    T* getRT(const TinyHandle& handle) {
        return rtRegistry.get<T>(handle);
    }

    template<typename T>
    const T* getRT(const TinyHandle& handle) const {
        return rtRegistry.get<T>(handle);
    }

    void* getRT(const TypeHandle& th) {
        return rtRegistry.get(th);
    }

    template<typename T>
    T* getRT(const TypeHandle& th) {
        assert(th.isType<T>() && "TypeHandle does not match requested type T");
        return rtRegistry.get<T>(th);
    }

    template<typename T>
    const T* getRT(const TypeHandle& th) const {
        assert(th.isType<T>() && "TypeHandle does not match requested type T");
        return rtRegistry.get<T>(th);
    }
};