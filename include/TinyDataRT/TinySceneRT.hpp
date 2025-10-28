#pragma once

#include "TinyExt/TinyFS.hpp"

#include "TinyDataRT/TinyNodeRT.hpp"
#include "TinyDataRT/TinyMeshRT.hpp"
#include "TinyDataRT/TinyAnime3D.hpp"
#include "TinyDataRT/TinySkeleton3D.hpp"

// TinySceneRT requirements
struct TinySceneReq {
    uint32_t maxFramesInFlight = 2; // If you messed this up the app just straight up jump off a cliff

    const TinyRegistry*   fsRegistry = nullptr; // For stuffs and things
    const TinyVK::Device* deviceVK = nullptr;   // For GPU resource creation

    VkDescriptorPool      skinDescPool   = VK_NULL_HANDLE;
    VkDescriptorSetLayout skinDescLayout = VK_NULL_HANDLE;

    bool valid() const {
        return  maxFramesInFlight > 0 &&
                fsRegistry != nullptr &&
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

    TinyNodeRT::SK3D has a TinyHandle pHandle that references the actual
    runtime Skeleton3D in the rtRegistry, changing it would cause undefined
    behavior. So rtComp<TinyNodeRT::SK3D> will return the Skeleton3D* instead

    */
    template<typename T>
    struct RTResolver { using type = T; }; // Most type return themselves
    template<typename T> using RTResolver_t = typename RTResolver<T>::type;

    // Special types
    template<> struct RTResolver<TinyNodeRT::SK3D> { using type = TinyRT_SK3D; };
    template<> struct RTResolver<TinyNodeRT::AN3D> { using type = TinyRT_AN3D; };
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
            return rtGet<TinyRT_SK3D>(compPtr->pHandle);
        } else if constexpr (type_eq<T, TinyNodeRT::AN3D>) {
            return rtGet<TinyRT_AN3D>(compPtr->pHandle);
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

        addMap3D<T>(nodeHandle);

        if constexpr (type_eq<T, TinyNodeRT::SK3D>) {
            return addSK3D_RT(compPtr);
        } else if constexpr (type_eq<T, TinyNodeRT::AN3D>) {
            return addAN3D_RT(compPtr);
        } else if constexpr (type_eq<T, TinyNodeRT::MR3D>) {
            return compPtr;
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
            rtRemove<TinyRT_SK3D>(compPtr->pHandle);
        } else if constexpr (type_eq<T, TinyNodeRT::AN3D>) {
            rtRemove<TinyRT_AN3D>(compPtr->pHandle);
        } else if constexpr (type_eq<T, TinyNodeRT::MR3D>) {
            // rtRemove<TinyRT::MR3D>(compPtr->pMeshRTHandle); // True implementation in future
        }

        rmMap3D<T>(nodeHandle);

        return node->remove<T>();
    }

    // -------- General update ---------

    void updateRecursive(TinyHandle nodeHandle = TinyHandle(), const glm::mat4& parentGlobalTransform = glm::mat4(1.0f));
    void updateTransform(TinyHandle nodeHandle = TinyHandle());
    void updateAnimation(float dTime);

    // --------- Specific component's data access ---------

    VkDescriptorSet nSkeleDescSet(TinyHandle nodeHandle) const {
        const TinyRT_SK3D* rtSkele = rtComp<TinyNodeRT::SK3D>(nodeHandle);
        return rtSkele ? rtSkele->descSet() : VK_NULL_HANDLE;
    }

    // UnorderedMap<TinyHandle, TinyHandle>& mapMR3D() { return mapMR3D_; }

    template<typename T>
    const UnorderedMap<TinyHandle, TinyHandle>& mapRT3D() const {
        if constexpr (type_eq<T, TinyNodeRT::MR3D>)      return mapMR3D_;
        else if constexpr (type_eq<T, TinyNodeRT::AN3D>) return mapAN3D_;
        else {
            static UnorderedMap<TinyHandle, TinyHandle> emptyMap;
            return emptyMap; // Empty map for unsupported types
        }
    }

    template<typename T>
    const TinyPool<TinyHandle>& poolRT3D() const {
        if constexpr (type_eq<T, TinyNodeRT::MR3D>)      return withMR3D_;
        else if constexpr (type_eq<T, TinyNodeRT::AN3D>) return withAN3D_;
        else {
            static TinyPool<TinyHandle> emptyPool;
            return emptyPool; // Empty pool for unsupported types
        }
    }

private:
    TinyPool<TinyNodeRT> nodes;
    TinyHandle rootHandle_;
    TinySceneReq sceneReq;   // Scene requirements
    TinyRegistry rtRegistry; // Runtime registry for this scene

    // ---------- Internal helpers ---------

    TinyNodeRT* nodeRef(TinyHandle nodeHandle) {
        return nodes.get(nodeHandle);
    }

    template<typename T>
    T* nodeComp(TinyHandle nodeHandle) {
        TinyNodeRT* node = nodes.get(nodeHandle);
        return node ? node->get<T>() : nullptr;
    }

    // Cache of specific nodes for easy access

    TinyPool<TinyHandle> withMR3D_;
    UnorderedMap<TinyHandle, TinyHandle> mapMR3D_;

    TinyPool<TinyHandle> withAN3D_;
    UnorderedMap<TinyHandle, TinyHandle> mapAN3D_;

    // ---------- Runtime component management ----------

    template<typename T>
    void addMap3D(TinyHandle nodeHandle) {
        auto& mapInsert = [this](auto& map, auto& pool, TinyHandle handle) {
            map[handle] = pool.add(handle);
        };

        if constexpr (type_eq<T, TinyNodeRT::MR3D>)  {
            mapInsert(mapMR3D_, withMR3D_, nodeHandle);
        } else if constexpr (type_eq<T, TinyNodeRT::AN3D>) {
            mapInsert(mapAN3D_, withAN3D_, nodeHandle);
        }
    }

    template<typename T>
    void rmMap3D(TinyHandle nodeHandle) {
        // Helpful helper function to remove from map and pool
        auto rmFromMapAndPool = [this](auto& map, auto& pool, TinyHandle handle) {
            auto it = map.find(handle);
            if (it != map.end()) {
                pool.instaRm(it->second);
                map.erase(it);
            }
        };

        if constexpr (type_eq<T, TinyNodeRT::MR3D>) {
            rmFromMapAndPool(mapMR3D_, withMR3D_, nodeHandle);
        } else if constexpr (type_eq<T, TinyNodeRT::AN3D>) {
            rmFromMapAndPool(mapAN3D_, withAN3D_, nodeHandle);
        }
    }

    TinyRT_SK3D* addSK3D_RT(TinyNodeRT::SK3D* compPtr) {
        TinyRT_SK3D rtSkele;
        rtSkele.init(sceneReq.deviceVK, sceneReq.fsRegistry, sceneReq.skinDescPool, sceneReq.skinDescLayout);
        // Repurpose pHandle to point to runtime skeleton
        compPtr->pHandle = rtAdd<TinyRT_SK3D>(std::move(rtSkele));

        // Return the runtime skeleton
        return rtGet<TinyRT_SK3D>(compPtr->pHandle);
    }

    TinyRT_AN3D* addAN3D_RT(TinyNodeRT::AN3D* compPtr) {
        TinyRT_AN3D rtAnime;
        compPtr->pHandle = rtAdd<TinyRT_AN3D>(std::move(rtAnime));

        return rtGet<TinyRT_AN3D>(compPtr->pHandle);
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
