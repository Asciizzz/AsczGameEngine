#pragma once

#include "TinyExt/TinyFS.hpp"

#include "TinyData/TinyNode.hpp"
#include "TinyData/TinyAnime.hpp"
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

private:

    // -------- writeComp's RTResolver ---------
    template<typename T>
    struct RTResolver { using type = T; }; // Most type return themselves

    // Special types
    template<> struct RTResolver<TinyNode::Skeleton> { using type = TinySkeletonRT; };
    template<> struct RTResolver<TinyNode::Animation> { using type = TinyAnimeRT; };

    template<typename T> using RTResolver_t = typename RTResolver<T>::type;

public:
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
    RTResolver_t<T>* writeComp(TinyHandle nodeHandle, const T& componentData = T()) {
        TinyNode* node = nodes.get(nodeHandle);
        if (!node) return nullptr;

        removeComp<T>(nodeHandle);
        node->add<T>(componentData);

        if constexpr (type_eq<T, TinyNode::Skeleton>) {
            return addSkeletonRT(nodeHandle);
        } else if constexpr (type_eq<T, TinyNode::Animation>) {
            return addAnimationRT(nodeHandle);
        } else { // Other types return themselves
            return nodeComp<T>(nodeHandle);
        }
    }

    template<typename T>
    void removeComp(TinyHandle nodeHandle) {
        TinyNode* node = nodes.get(nodeHandle);
        if (!node || !node->has<T>()) return;

        if constexpr (type_eq<T, TinyNode::Skeleton>) {
            TinySkeletonRT* rtSkele = nSkeletonRT(nodeHandle);
            if (rtSkele) rRemove<TinySkeletonRT>(rtSkele->skeleHandle);
        }

        node->remove<T>();
    }

    template<typename T>
    T copyComp(TinyHandle nodeHandle) const {
        const TinyNode* node = nodes.get(nodeHandle);
        return node ? node->getCopy<T>() : T();
    }

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

    // --------- Specific component's data access ---------

    // You are not allowed to modify the node component identity directly
    // BUT, you are allowed to modify the data inside the component

    // Exposable runtime access
    TinyNode::Transform* nTransform(TinyHandle nodeHandle);
    const TinyNode::Transform* nTransform(TinyHandle nodeHandle) const;

    TinySkeletonRT* nSkeletonRT(TinyHandle nodeHandle);
    const TinySkeletonRT* nSkeletonRT(TinyHandle nodeHandle) const;
    const TinySkeleton* nSkeleton(TinyHandle nodeHandle) const;
    VkDescriptorSet nSkeleDescSet(TinyHandle nodeHandle) const;

    // TinyAnimeRT* nAnimationRT(TinyHandle nodeHandle);

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

    // ---------- Runtime component management ----------

    TinySkeletonRT* addSkeletonRT(TinyHandle nodeHandle);
    TinyAnimeRT* addAnimationRT(TinyHandle nodeHandle);

    // ---------- Runtime registry access (private) ----------

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
