#pragma once

#include "TinyExt/TinyFS.hpp"

#include "TinyDataRT/TinyNodeRT.hpp"
#include "TinyDataRT/TinyMeshRT.hpp"
#include "TinyDataRT/TinyAnimeRT.hpp"
#include "TinyDataRT/TinySkeleton3D.hpp"

// TinySceneRT requirements
struct TinySceneReq {
    const TinyRegistry*   fsRegistry = nullptr;
    const TinyVK::Device* deviceVK = nullptr; // For GPU resource creation

    VkDescriptorPool      skinDescPool   = VK_NULL_HANDLE;
    VkDescriptorSetLayout skinDescLayout = VK_NULL_HANDLE;

    bool valid() const {
        return  fsRegistry != nullptr &&
                deviceVK != nullptr &&
                skinDescPool   != VK_NULL_HANDLE &&
                skinDescLayout != VK_NULL_HANDLE;
    }
};

struct TinySceneRT {
private:

    // -------- writeComp's RTResolver ---------

    /* The idea:

    TinyNode's component themselves should not be modifiable at runtime, as 
    they directly reference the runtime data in rtRegistry

    So instead, manipulations and retrievals of components should be done through
    the TinySceneRT's rtComp and writeComp functions, which will return the
    appropriate runtime component pointers, not the identity component pointers.

    For example:

    TinyNodeRT::SK3D has a TinyHandle pSkeleHandle that references the actual
    runtime Skeleton3D in the rtRegistry, changing it would cause undefined
    behavior. So rtComp<TinyNodeRT::SK3D> will return the Skeleton3D* instead

    */
    template<typename T>
    struct RTResolver { using type = T; }; // Most type return themselves
    template<typename T> using RTResolver_t = typename RTResolver<T>::type;

    // Special types
    template<> struct RTResolver<TinyNodeRT::SK3D> { using type = TinyRT_SK3D; };
    template<> struct RTResolver<TinyNodeRT::AN3D> { using type = TinyAnimeRT; };
    // template<> struct RTResolver<TinyNodeRT::MR3D> { using type = TinyRT::MeshRT; }; // Will be added very soon

public:
    std::string name;

    TinySceneRT(const std::string& sceneName = "New Scene") : name(sceneName) {}

    TinySceneRT(const TinySceneRT&) = delete;
    TinySceneRT& operator=(const TinySceneRT&) = delete;

    TinySceneRT(TinySceneRT&&) = default;
    TinySceneRT& operator=(TinySceneRT&&) = default;

    // --------- Core management ---------

    TinyHandle addRoot(const std::string& nodeName = "Root");
    void setRoot(TinyHandle handle) { rootHandle_ = handle; }
    TinyHandle rootHandle() const { return rootHandle_; }

    void setSceneReq(const TinySceneReq& req) {
        if (!req.valid()) throw std::invalid_argument("Invalid TinySceneReq provided to TinySceneRT");
        sceneReq = req;
    }

    bool valid() const { return sceneReq.valid(); }

    // --------- Node management ---------

    // No add node by TinyNodeRT because of component logic
    TinyHandle addNode(const std::string& nodeName = "New Node", TinyHandle parentHandle = TinyHandle());
    TinyHandle addNodeRaw(const std::string& nodeName = "New Node");

    bool removeNode(TinyHandle nodeHandle, bool recursive = true);
    bool flattenNode(TinyHandle nodeHandle);
    bool reparentNode(TinyHandle nodeHandle, TinyHandle newParentHandle);
    bool renameNode(TinyHandle nodeHandle, const std::string& newName);

    // TinyNodeRT* node(TinyHandle nodeHandle);
    const TinyNodeRT* node(TinyHandle nodeHandle) const;
    const std::vector<TinyNodeRT>& nodeView() const;
    bool nodeValid(TinyHandle nodeHandle) const;
    bool nodeOccupied(uint32_t index) const;
    TinyHandle nodeHandle(uint32_t index) const;
    uint32_t nodeCount() const;

    TinyHandle nodeParent(TinyHandle nodeHandle) const;
    std::vector<TinyHandle> nodeChildren(TinyHandle nodeHandle) const;
    bool setNodeParent(TinyHandle nodeHandle, TinyHandle newParentHandle);
    bool setNodeChildren(TinyHandle nodeHandle, const std::vector<TinyHandle>& newChildren);

    void addScene(const TinySceneRT* from, TinyHandle parentHandle = TinyHandle());

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


    // -------- Component management --------- 

    // Retrieve runtime-resolved component pointer (return runtime component instead of node identity component)
    template<typename T>
    RTResolver_t<T>* rtComp(TinyHandle nodeHandle) {
        TinyNodeRT* node = nodes.get(nodeHandle);
        if (!node || !node->has<T>()) return nullptr;

        T* compPtr = node->get<T>();

        if constexpr (type_eq<T, TinyNodeRT::SK3D>) {
            return rtGet<TinyRT_SK3D>(compPtr->pSkeleHandle);
        } else if constexpr (type_eq<T, TinyNodeRT::AN3D>) {
            return rtGet<TinyAnimeRT>(compPtr->pAnimeHandle);
        } else { // Other types return themselves
            return compPtr;
        }
    }

    template<typename T>
    const RTResolver_t<T>* rtComp(TinyHandle nodeHandle) const {
        return const_cast<TinySceneRT*>(this)->rtComp<T>(nodeHandle);
    }

    template<typename T>
    RTResolver_t<T>* writeComp(TinyHandle nodeHandle) {
        TinyNodeRT* node = nodes.get(nodeHandle);
        if (!node) return nullptr;

        removeComp<T>(nodeHandle);
        T* compPtr = node->add<T>();

        if constexpr (type_eq<T, TinyNodeRT::SK3D>) {
            return addSK3D_RT(nodeHandle);
        } else if constexpr (type_eq<T, TinyNodeRT::AN3D>) {
            return addAN3D_RT(nodeHandle);
        } else if constexpr (type_eq<T, TinyNodeRT::MR3D>) {
            // return addMeshRenderRT(nodeHandle); // Will be added very soon
            return addMR3D(nodeHandle);
        } else { // Other types return themselves
            return compPtr;
        }
    }

    template<typename T>
    bool removeComp(TinyHandle nodeHandle) {
        TinyNodeRT* node = nodes.get(nodeHandle);
        if (!node || !node->has<T>()) return false;

        T* compPtr = node->get<T>();

        if constexpr (type_eq<T, TinyNodeRT::SK3D>) {
            rtRemove<TinyRT_SK3D>(compPtr->pSkeleHandle);
        } else if constexpr (type_eq<T, TinyNodeRT::AN3D>) {
            rtRemove<TinyAnimeRT>(compPtr->pAnimeHandle);
        } else if constexpr (type_eq<T, TinyNodeRT::MR3D>) {
            rmMR3D(nodeHandle);
        }

        return node->remove<T>();
    }

    // -------- General update ---------

    void updateRecursive(TinyHandle nodeHandle = TinyHandle(), const glm::mat4& parentGlobalTransform = glm::mat4(1.0f));
    void updateTransform(TinyHandle nodeHandle = TinyHandle());

    // --------- Specific component's data access ---------

    VkDescriptorSet nSkeleDescSet(TinyHandle nodeHandle) const;
    UnorderedMap<TinyHandle, TinyHandle>& nodeToMR3DMap() { return nodeToMR3D_; }

private:
    TinyPool<TinyNodeRT> nodes;
    TinyHandle rootHandle_;
    TinySceneReq sceneReq;   // Scene requirements
    TinyRegistry rtRegistry; // Runtime registry for this scene

    // Cache of MR3D nodes for easy access
    TinyPool<TinyHandle> meshRenderList_;
    UnorderedMap<TinyHandle, TinyHandle> nodeToMR3D_;

    // ---------- Internal helpers ---------

    TinyNodeRT* nodeRef(TinyHandle nodeHandle) {
        return nodes.get(nodeHandle);
    }

    template<typename T>
    T* nodeComp(TinyHandle nodeHandle) {
        TinyNodeRT* node = nodes.get(nodeHandle);
        return node ? node->get<T>() : nullptr;
    }

    // ---------- Runtime component management ----------

    TinyRT_SK3D* addSK3D_RT(TinyHandle nodeHandle);
    TinyAnimeRT* addAN3D_RT(TinyHandle nodeHandle);

    TinyNodeRT::MR3D* addMR3D(TinyHandle nodeHandle) {
        nodeToMR3D_[nodeHandle] = meshRenderList_.add(nodeHandle);
        return nodeRef(nodeHandle)->get<TinyNodeRT::MR3D>();
    }

    void rmMR3D(TinyHandle nodeHandle) {
        auto it = nodeToMR3D_.find(nodeHandle);
        if (it != nodeToMR3D_.end()) {
            meshRenderList_.instaRm(it->second);
            nodeToMR3D_.erase(it);
        }
    }

    // ---------- Runtime registry access (private) ----------

    // Access node by index, only for internal use
    TinyNodeRT* fromIndex(uint32_t index) {
        return nodes.get(nodeHandle(index));
    }

    const TinyNodeRT* fromIndex(uint32_t index) const {
        return nodes.get(nodeHandle(index));
    }

    template<typename T>
    TinyHandle rtAdd(T&& data) {
        return rtRegistry.add<T>(std::forward<T>(data)).handle;
    }


    template<typename T> struct DeferredRm : std::false_type {};
    template<> struct DeferredRm<TinyRT_SK3D> : std::true_type {};

    template<typename T>
    void rtRemove(const TinyHandle& handle) {
        if constexpr (DeferredRm<T>::value) rtRegistry.tQueueRm<T>(handle);
        else                                rtRegistry.tInstaRm<T>(handle);
    }
};
