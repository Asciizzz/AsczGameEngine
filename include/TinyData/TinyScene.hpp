#pragma once

#include "TinyExt/TinyFS.hpp"

#include "TinyData/TinyNode.hpp"
#include "TinyData/TinySkeleton.hpp"

// TinyScene requirements
struct TinySceneReq {
    TinyFS*               fs = nullptr;          // Pointer to filesystem for resource lookups and registry management
    const TinyVK::Device* device     = nullptr;  // For GPU resource creation

    VkDescriptorPool      skinDescPool   = VK_NULL_HANDLE;
    VkDescriptorSetLayout skinDescLayout = VK_NULL_HANDLE;

    bool valid() const {
        return  fs != nullptr && device != nullptr &&
                skinDescPool   != VK_NULL_HANDLE &&
                skinDescLayout != VK_NULL_HANDLE;
    }
};

struct TinyScene {
    std::string name;

    TinyScene(const std::string& sceneName = "New Scene") : name(sceneName) {}

    TinyScene(const TinyScene&) = delete;
    TinyScene& operator=(const TinyScene&) = delete;

    TinyScene(TinyScene&&) = default;
    TinyScene& operator=(TinyScene&&) = default;

    // --------- Core management ---------

    TinyHandle addRoot(const std::string& nodeName = "Root");
    void setRoot(TinyHandle handle) { rootHandle_ = handle; }
    TinyHandle rootHandle() const { return rootHandle_; }

    void setSceneReq(const TinySceneReq& req) {
        if (!req.valid()) throw std::invalid_argument("Invalid TinySceneReq provided to TinyScene");
        sceneReq = req;
    }

    bool valid() const { return sceneReq.valid(); }

    // --------- Node management ---------

    // No add node by TinyNode because of component logic
    TinyHandle addNode(const std::string& nodeName = "New Node", TinyHandle parentHandle = TinyHandle());
    TinyHandle addNodeRaw(const std::string& nodeName = "New Node");

    bool removeNode(TinyHandle nodeHandle, bool recursive = true);
    bool flattenNode(TinyHandle nodeHandle);
    bool reparentNode(TinyHandle nodeHandle, TinyHandle newParentHandle);
    bool renameNode(TinyHandle nodeHandle, const std::string& newName);

    // TinyNode* node(TinyHandle nodeHandle);
    const TinyNode* node(TinyHandle nodeHandle) const;
    const std::vector<TinyNode>& nodeView() const;
    bool nodeValid(TinyHandle nodeHandle) const;
    bool nodeOccupied(uint32_t index) const;
    TinyHandle nodeHandle(uint32_t index) const;
    uint32_t nodeCount() const;

    TinyHandle nodeParent(TinyHandle nodeHandle) const;
    std::vector<TinyHandle> nodeChildren(TinyHandle nodeHandle) const;
    bool setNodeParent(TinyHandle nodeHandle, TinyHandle newParentHandle);
    bool setNodeChildren(TinyHandle nodeHandle, const std::vector<TinyHandle>& newChildren);

    void addScene(TinyHandle sceneHandle, TinyHandle parentHandle = TinyHandle());
    
    // -------- General update ---------
    
    void updateRecursive(TinyHandle nodeHandle = TinyHandle(), const glm::mat4& parentGlobalTransform = glm::mat4(1.0f));
    void update(TinyHandle nodeHandle = TinyHandle());

    // -------- Component management --------- 

    template<typename T>
    const T* nodeComp(TinyHandle nodeHandle) const {
        const TinyNode* node = nodes.get(nodeHandle);
        if (!node) return nullptr;

        return node->get<T>();
    }

    template<typename T>
    void nodeAddComp(TinyHandle nodeHandle, const T& componentData = T()) {
        TinyNode* node = nodes.get(nodeHandle);
        if (!node) return;

        if constexpr (std::is_same_v<T, TinyNode::Skeleton>) {
            nodeAddCompSkeleton(nodeHandle, componentData.rtSkeleHandle);
        } else {
            node->add<T>(componentData);
        }
    }

    template<typename T>
    void nodeRemoveComp(TinyHandle nodeHandle) {
        TinyNode* node = nodes.get(nodeHandle);
        if (!node) return;

        if constexpr (std::is_same_v<T, TinyNode::Skeleton>) {
            nodeRemoveCompSkeleton(nodeHandle);
        } else {
            node->remove<T>();
        }
    }

    template<typename T>
    T copyComp(TinyHandle nodeHandle) const {
        const TinyNode* node = nodes.get(nodeHandle);
        if (!node) return T(); // Empty/default

        return node->getCopy<T>();
    }

    // --------- Specific component logic ---------

    VkDescriptorSet getNodeSkeletonDescSet(TinyHandle nodeHandle) const;

    // --------- Runtime registry access (public) ---------

    template<typename T>
    T* rGet(const TinyHandle& handle) {
        if (!sceneReq.fs) return nullptr;
        return fs()->rGet<T>(handle);
    }

    template<typename T>
    const T* rGet(const TinyHandle& handle) const {
        if (!sceneReq.fs) return nullptr;
        return fs()->rGet<T>(handle);
    }

private:
    TinyPool<TinyNode> nodes;
    TinyHandle rootHandle_{};

    TinySceneReq sceneReq;   // Scene requirements

    // --------- Complex component logic ---------

    // Convenience access
    TinyFS* fs() { return sceneReq.fs; }
    const TinyFS* fs() const { return sceneReq.fs; }

    // Non-const access only for internal use
    template<typename T>
    T* nodeComp(TinyHandle nodeHandle) {
        TinyNode* node = nodes.get(nodeHandle);
        if (!node) return nullptr;

        return node->get<T>();
    }

    TinyHandle nodeAddCompSkeleton(TinyHandle nodeHandle, TinyHandle skeletonHandle);
    void nodeRemoveCompSkeleton(TinyHandle nodeHandle);

    // ---------- Runtime registry access via TinyFS (private) ----------

    // Access node by index, only for internal use
    TinyNode* fromIndex(uint32_t index) {
        return nodes.get(nodeHandle(index));
    }

    const TinyNode* fromIndex(uint32_t index) const {
        return nodes.get(nodeHandle(index));
    }

    template<typename T>
    TinyHandle rAdd(T& data) {
        if (!sceneReq.fs) return TinyHandle{};
        return fs()->rAdd<T>(std::move(data)).handle;
    }

    template<typename T>
    void rRemove(const TinyHandle& handle) {
        if (!sceneReq.fs) return;
        fs()->rRemove<T>(handle);
    }

    void* rGet(const TypeHandle& th) {
        if (!sceneReq.fs) return nullptr;
        return fs()->rGet(th);
    }

    template<typename T>
    T* rGet(const TypeHandle& th) {
        if (!sceneReq.fs) return nullptr;
        return fs()->rGet<T>(th);
    }

    template<typename T>
    const T* rGet(const TypeHandle& th) const {
        if (!sceneReq.fs) return nullptr;
        return fs()->rGet<T>(th);
    }



    // Debug function
    void printNodeHierarchy(TinyHandle nodeHandle = TinyHandle(), int depth = 0) const {
        const TinyNode* node = nodes.get(nodeHandle);
        if (!node) return;

        for (int i = 0; i < depth; ++i) {
            printf("  ");
        }
        printf("- %s (Parent: %u_%u)\n", node->name.c_str(), node->parentHandle.index, node->parentHandle.version);

        for (const TinyHandle& childHandle : node->childrenHandles) {
            printNodeHierarchy(childHandle, depth + 1);
        }
    }
};
