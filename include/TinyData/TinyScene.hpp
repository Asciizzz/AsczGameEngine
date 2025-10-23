#pragma once

#include "TinyExt/TinyFS.hpp"

#include "TinyData/TinyNode.hpp"
#include "TinyData/TinyAnime.hpp"
#include "TinyData/TinySkeleton.hpp"

// TinyScene requirements
struct TinySceneReq {
    const TinyVK::Device* deviceVK = nullptr; // For GPU resource creation

    VkDescriptorPool      skinDescPool   = VK_NULL_HANDLE;
    VkDescriptorSetLayout skinDescLayout = VK_NULL_HANDLE;

    bool valid() const {
        return  deviceVK != nullptr &&
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

    TinyScene(const std::string& sceneName = "New Scene") : name(sceneName) {
        TinyPool<TinySkeletonRT>& skeleRTPool = rtRegistry.make<TinySkeletonRT>();
        skeleRTPool.alloc(1024); // Preallocate 1024 runtime skeletons
    }

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

    void addScene(const TinyScene* from, TinyHandle parentHandle = TinyHandle());
    
    // -------- General update ---------
    
    void updateRecursive(TinyHandle nodeHandle = TinyHandle(), const glm::mat4& parentGlobalTransform = glm::mat4(1.0f));
    void update(TinyHandle nodeHandle = TinyHandle());

    // -------- Component management --------- 

    template<typename T>
    RTResolver_t<T>* nodeComp(TinyHandle nodeHandle) {
        TinyNode* node = nodes.get(nodeHandle);
        if (!node) return nullptr;

        if constexpr (type_eq<T, TinyNode::Skeleton>) {
            TinyNode::Skeleton* compPtr = node->get<TinyNode::Skeleton>();
            return compPtr ? rtGet<TinySkeletonRT>(compPtr->pSkeleHandle) : nullptr;
        } else if constexpr (type_eq<T, TinyNode::Animation>) {
            TinyNode::Animation* compPtr = node->get<TinyNode::Animation>();
            return compPtr ? rtGet<TinyAnimeRT>(compPtr->pAnimeHandle) : nullptr;
        } else { // Other types return themselves
            return node->get<T>();
        }
    }

    template<typename T>
    const RTResolver_t<T>* nodeComp(TinyHandle nodeHandle) const {
        return const_cast<TinyScene*>(this)->nodeComp<T>(nodeHandle);
    }

    template<typename T>
    RTResolver_t<T>* writeComp(TinyHandle nodeHandle) {
        TinyNode* node = nodes.get(nodeHandle);
        if (!node) return nullptr;

        removeComp<T>(nodeHandle);
        node->add<T>();

        if constexpr (type_eq<T, TinyNode::Skeleton>) {
            return addSkeletonRT(nodeHandle);
        } else if constexpr (type_eq<T, TinyNode::Animation>) {
            return addAnimationRT(nodeHandle);
        } else { // Other types return themselves
            return nodeCompRaw<T>(nodeHandle);
        }
    }

    template<typename T>
    bool removeComp(TinyHandle nodeHandle) {
        TinyNode* node = nodes.get(nodeHandle);
        if (!node || !node->has<T>()) return false;

        if constexpr (type_eq<T, TinyNode::Skeleton>) {
            TinyNode::Skeleton* compRawPtr = nodeCompRaw<TinyNode::Skeleton>(nodeHandle);
            if (compRawPtr) rtRemove<TinySkeletonRT>(compRawPtr->pSkeleHandle);
        }

        return node->remove<T>();
    }

    // --------- Runtime registry access (public) ---------

    template<typename T>
    T* rtGet(const TinyHandle& handle) {
        return rtRegistry.get<T>(handle);
    }

    template<typename T>
    const T* rtGet(const TinyHandle& handle) const {
        return rtRegistry.get<T>(handle);
    }

    void* rtGet(const TypeHandle& th) {
        return rtRegistry.get(th);
    }

    template<typename T>
    T* rtGet(const TypeHandle& th) {
        return rtRegistry.get<T>(th);
    }

    template<typename T>
    const T* rtGet(const TypeHandle& th) const {
        return rtRegistry.get<T>(th);
    }

    template<typename T>
    bool rtTHasPendingRms() const {
        return rtRegistry.tHasPendingRms<T>();
    }

    template<typename T>
    void rtTFlushAllRms() {
        rtRegistry.tFlushAllRms<T>();
    }


    bool rtHasPendingRms() const {
        return // Add more in the future if needed
            rtTHasPendingRms<TinySkeletonRT>(); // &&
            // rtTHasPendingRms<TinyAnimeRT>();
    }

    void rtFlushAllRms() {
        rtTFlushAllRms<TinySkeletonRT>();
        // rtTFlushAllRms<TinyAnimeRT>();
    }

    // --------- Specific component's data access ---------

    VkDescriptorSet nSkeleDescSet(TinyHandle nodeHandle) const;

private:
    TinyPool<TinyNode> nodes;
    TinyHandle rootHandle_{};

    TinySceneReq sceneReq;   // Scene requirements

    TinyRegistry rtRegistry; // Local runtime registry for this scene

    // Non-const access only for internal use
    template<typename T>
    T* nodeCompRaw(TinyHandle nodeHandle) {
        TinyNode* node = nodes.get(nodeHandle);
        if (!node) return nullptr;

        return node->get<T>();
    }

    template<typename T>
    const T* nodeCompRaw(TinyHandle nodeHandle) const {
        const TinyNode* node = nodes.get(nodeHandle);
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
    TinyHandle rtAdd(T& data) {
        return rtRegistry.add<T>(data).handle;
    }

    template<typename T>
    void rtRemove(const TinyHandle& handle) {
        // CRITICAL: Special handling for dangerous type (vulkan related)
        if constexpr (type_eq<T, TinySkeletonRT>) {
            rtRegistry.tQueueRm<T>(handle);
        } else {
            rtRegistry.tInstaRm<T>(handle);
        }
    }

};
