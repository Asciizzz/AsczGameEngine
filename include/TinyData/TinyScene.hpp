#pragma once

#include "TinyData/TinyNode.hpp"
#include "TinyExt/TinyRegistry.hpp"

struct TinySkelePlaceholder {};

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

    // TinyNode* node(TinyHandle nodeHandle);
    const TinyNode* node(TinyHandle nodeHandle) const;
    const std::vector<TinyNode>& nodeView() const;
    bool nodeValid(TinyHandle nodeHandle) const;
    bool nodeOccupied(uint32_t index) const;
    uint32_t nodeCount() const;

    TinyHandle nodeParent(TinyHandle nodeHandle) const;
    std::vector<TinyHandle> nodeChildren(TinyHandle nodeHandle) const;
    bool setNodeParent(TinyHandle nodeHandle, TinyHandle newParentHandle);
    bool setNodeChildren(TinyHandle nodeHandle, const std::vector<TinyHandle>& newChildren);

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
            compPtr->rtSkeleHandle = addRT<TinySkelePlaceholder>(TinySkelePlaceholder());
        }

        // Other component-specific logic can go here
    }

    template<typename T>
    void nodeRemoveComp(TinyHandle nodeHandle) {
        TinyNode* node = nodes.get(nodeHandle);
        if (!node) return;

        // Resolve component removal logic
        T* compPtr = node->get<T>();
        if (!compPtr) return;

        if constexpr (std::is_same_v<T, TinyNode::Skeleton>) {
            removeRT<TinySkelePlaceholder>(compPtr->rtSkeleHandle);
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
    TinyHandle addRT(T& data) {
        return rtRegistry.add<T>(std::move(data)).handle;
    }

    template<typename T>
    void removeRT(const TinyHandle& handle) {
        rtRegistry.remove<T>(handle);
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