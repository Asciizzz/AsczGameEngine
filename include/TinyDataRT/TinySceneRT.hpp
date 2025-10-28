#pragma once

#include "tinyExt/tinyFS.hpp"

#include "tinyDataRT/tinyNodeRT.hpp"
#include "tinyDataRT/tinyMeshRT.hpp"
#include "tinyDataRT/tinyAnime3D.hpp"
#include "tinyDataRT/tinySkeleton3D.hpp"

// tinySceneRT requirements
struct tinySceneReq {
    uint32_t maxFramesInFlight = 2; // If you messed this up the app just straight up jump off a cliff

    const tinyRegistry*   fsRegistry = nullptr; // For stuffs and things
    const tinyVK::Device* deviceVK = nullptr;   // For GPU resource creation

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

struct tinySceneRT {
private:

    // -------- writeComp's RTResolver ---------

    /* The idea:

    tinyNode's component themselves should not be modifiable at runtime, as 
    they directly reference the runtime data in rtRegistry

    So instead, manipulations and retrievals of components should be done through
    the tinySceneRT's rtComp and writeComp functions, which will return the
    appropriate runtime component pointers, not the identity component pointers.

    For example:

    tinyNodeRT::SK3D has a tinyHandle pHandle that references the actual
    runtime Skeleton3D in the rtRegistry, changing it would cause undefined
    behavior. So rtComp<tinyNodeRT::SK3D> will return the Skeleton3D* instead

    */
    template<typename T>
    struct RTResolver { using type = T; }; // Most type return themselves
    template<typename T> using RTResolver_t = typename RTResolver<T>::type;

    // Special types
    template<> struct RTResolver<tinyNodeRT::SK3D> { using type = tinyRT_SK3D; };
    template<> struct RTResolver<tinyNodeRT::AN3D> { using type = tinyRT_AN3D; };
    // template<> struct RTResolver<tinyNodeRT::MR3D> { using type = tinyRT::MeshRT; }; // Will be added very soon

public:
    std::string name;

    tinySceneRT(const std::string& sceneName = "New Scene") : name(sceneName) {}

    tinySceneRT(const tinySceneRT&) = delete;
    tinySceneRT& operator=(const tinySceneRT&) = delete;

    tinySceneRT(tinySceneRT&&) = default;
    tinySceneRT& operator=(tinySceneRT&&) = default;

    // --------- Core management ---------

    tinyHandle addRoot(const std::string& nodeName = "Root");
    void setRoot(tinyHandle handle) { rootHandle_ = handle; }
    tinyHandle rootHandle() const { return rootHandle_; }

    void setSceneReq(const tinySceneReq& req) {
        if (!req.valid()) throw std::invalid_argument("Invalid tinySceneReq provided to tinySceneRT");
        sceneReq = req;
    }

    bool valid() const { return sceneReq.valid(); }

    // --------- Node management ---------

    // No add node by tinyNodeRT because of component logic
    tinyHandle addNode(const std::string& nodeName = "New Node", tinyHandle parentHandle = tinyHandle());
    tinyHandle addNodeRaw(const std::string& nodeName = "New Node");

    bool removeNode(tinyHandle nodeHandle, bool recursive = true);
    bool flattenNode(tinyHandle nodeHandle);
    bool reparentNode(tinyHandle nodeHandle, tinyHandle newParentHandle);
    bool renameNode(tinyHandle nodeHandle, const std::string& newName);

    // tinyNodeRT* node(tinyHandle nodeHandle);
    const tinyNodeRT* node(tinyHandle nodeHandle) const;
    const std::vector<tinyNodeRT>& nodeView() const;
    bool nodeValid(tinyHandle nodeHandle) const;
    bool nodeOccupied(uint32_t index) const;
    tinyHandle nodeHandle(uint32_t index) const;
    uint32_t nodeCount() const;

    tinyHandle nodeParent(tinyHandle nodeHandle) const;
    std::vector<tinyHandle> nodeChildren(tinyHandle nodeHandle) const;
    bool setNodeParent(tinyHandle nodeHandle, tinyHandle newParentHandle);
    bool setNodeChildren(tinyHandle nodeHandle, const std::vector<tinyHandle>& newChildren);

    void addScene(const tinySceneRT* from, tinyHandle parentHandle = tinyHandle());

    // --------- Runtime registry access (public) ---------

    template<typename T>
    T* rtGet(const tinyHandle& handle) {
        return rtRegistry.get<T>(handle);
    }

    template<typename T>
    const T* rtGet(const tinyHandle& handle) const {
        return rtRegistry.get<T>(handle);
    }

    void* rtGet(const typeHandle& th) {
        return rtRegistry.get(th);
    }

    template<typename T>
    T* rtGet(const typeHandle& th) {
        return rtRegistry.get<T>(th);
    }

    template<typename T>
    const T* rtGet(const typeHandle& th) const {
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
    RTResolver_t<T>* rtComp(tinyHandle nodeHandle) {
        tinyNodeRT* node = nodes.get(nodeHandle);
        if (!node || !node->has<T>()) return nullptr;

        T* compPtr = node->get<T>();

        if constexpr (type_eq<T, tinyNodeRT::SK3D>) {
            return rtGet<tinyRT_SK3D>(compPtr->pHandle);
        } else if constexpr (type_eq<T, tinyNodeRT::AN3D>) {
            return rtGet<tinyRT_AN3D>(compPtr->pHandle);
        } else { // Other types return themselves
            return compPtr;
        }
    }

    template<typename T>
    const RTResolver_t<T>* rtComp(tinyHandle nodeHandle) const {
        return const_cast<tinySceneRT*>(this)->rtComp<T>(nodeHandle);
    }

    template<typename T>
    RTResolver_t<T>* writeComp(tinyHandle nodeHandle) {
        tinyNodeRT* node = nodes.get(nodeHandle);
        if (!node) return nullptr;

        removeComp<T>(nodeHandle);
        T* compPtr = node->add<T>();

        addMap3D<T>(nodeHandle);

        if constexpr (type_eq<T, tinyNodeRT::SK3D>) {
            return addSK3D_RT(compPtr);
        } else if constexpr (type_eq<T, tinyNodeRT::AN3D>) {
            return addAN3D_RT(compPtr);
        } else if constexpr (type_eq<T, tinyNodeRT::MR3D>) {
            return compPtr;
        } else { // Other types return themselves
            return compPtr;
        }
    }

    template<typename T>
    bool removeComp(tinyHandle nodeHandle) {
        tinyNodeRT* node = nodes.get(nodeHandle);
        if (!node || !node->has<T>()) return false;

        T* compPtr = node->get<T>();

        if constexpr (type_eq<T, tinyNodeRT::SK3D>) {
            rtRemove<tinyRT_SK3D>(compPtr->pHandle);
        } else if constexpr (type_eq<T, tinyNodeRT::AN3D>) {
            rtRemove<tinyRT_AN3D>(compPtr->pHandle);
        } else if constexpr (type_eq<T, tinyNodeRT::MR3D>) {
            // rtRemove<tinyRT::MR3D>(compPtr->pMeshRTHandle); // True implementation in future
        }

        rmMap3D<T>(nodeHandle);

        return node->remove<T>();
    }

    // -------- General update ---------

    void updateRecursive(tinyHandle nodeHandle = tinyHandle(), const glm::mat4& parentGlobalTransform = glm::mat4(1.0f));
    void updateTransform(tinyHandle nodeHandle = tinyHandle());
    void updateAnimation(float dTime);

    // --------- Specific component's data access ---------

    VkDescriptorSet nSkeleDescSet(tinyHandle nodeHandle) const {
        const tinyRT_SK3D* rtSkele = rtComp<tinyNodeRT::SK3D>(nodeHandle);
        return rtSkele ? rtSkele->descSet() : VK_NULL_HANDLE;
    }

    // UnorderedMap<tinyHandle, tinyHandle>& mapMR3D() { return mapMR3D_; }

    template<typename T>
    const UnorderedMap<tinyHandle, tinyHandle>& mapRT3D() const {
        if constexpr (type_eq<T, tinyNodeRT::MR3D>)      return mapMR3D_;
        else if constexpr (type_eq<T, tinyNodeRT::AN3D>) return mapAN3D_;
        else {
            static UnorderedMap<tinyHandle, tinyHandle> emptyMap;
            return emptyMap; // Empty map for unsupported types
        }
    }

    template<typename T>
    const tinyPool<tinyHandle>& poolRT3D() const {
        if constexpr (type_eq<T, tinyNodeRT::MR3D>)      return withMR3D_;
        else if constexpr (type_eq<T, tinyNodeRT::AN3D>) return withAN3D_;
        else {
            static tinyPool<tinyHandle> emptyPool;
            return emptyPool; // Empty pool for unsupported types
        }
    }

private:
    tinyPool<tinyNodeRT> nodes;
    tinyHandle rootHandle_;
    tinySceneReq sceneReq;   // Scene requirements
    tinyRegistry rtRegistry; // Runtime registry for this scene

    // ---------- Internal helpers ---------

    tinyNodeRT* nodeRef(tinyHandle nodeHandle) {
        return nodes.get(nodeHandle);
    }

    template<typename T>
    T* nodeComp(tinyHandle nodeHandle) {
        tinyNodeRT* node = nodes.get(nodeHandle);
        return node ? node->get<T>() : nullptr;
    }

    // Cache of specific nodes for easy access

    tinyPool<tinyHandle> withMR3D_;
    UnorderedMap<tinyHandle, tinyHandle> mapMR3D_;

    tinyPool<tinyHandle> withAN3D_;
    UnorderedMap<tinyHandle, tinyHandle> mapAN3D_;

    // ---------- Runtime component management ----------

    template<typename T>
    void addMap3D(tinyHandle nodeHandle) {
        auto& mapInsert = [this](auto& map, auto& pool, tinyHandle handle) {
            map[handle] = pool.add(handle);
        };

        if constexpr (type_eq<T, tinyNodeRT::MR3D>)  {
            mapInsert(mapMR3D_, withMR3D_, nodeHandle);
        } else if constexpr (type_eq<T, tinyNodeRT::AN3D>) {
            mapInsert(mapAN3D_, withAN3D_, nodeHandle);
        }
    }

    template<typename T>
    void rmMap3D(tinyHandle nodeHandle) {
        // Helpful helper function to remove from map and pool
        auto rmFromMapAndPool = [this](auto& map, auto& pool, tinyHandle handle) {
            auto it = map.find(handle);
            if (it != map.end()) {
                pool.instaRm(it->second);
                map.erase(it);
            }
        };

        if constexpr (type_eq<T, tinyNodeRT::MR3D>) {
            rmFromMapAndPool(mapMR3D_, withMR3D_, nodeHandle);
        } else if constexpr (type_eq<T, tinyNodeRT::AN3D>) {
            rmFromMapAndPool(mapAN3D_, withAN3D_, nodeHandle);
        }
    }

    tinyRT_SK3D* addSK3D_RT(tinyNodeRT::SK3D* compPtr) {
        tinyRT_SK3D rtSkele;
        rtSkele.init(sceneReq.deviceVK, sceneReq.fsRegistry, sceneReq.skinDescPool, sceneReq.skinDescLayout);
        // Repurpose pHandle to point to runtime skeleton
        compPtr->pHandle = rtAdd<tinyRT_SK3D>(std::move(rtSkele));

        // Return the runtime skeleton
        return rtGet<tinyRT_SK3D>(compPtr->pHandle);
    }

    tinyRT_AN3D* addAN3D_RT(tinyNodeRT::AN3D* compPtr) {
        tinyRT_AN3D rtAnime;
        compPtr->pHandle = rtAdd<tinyRT_AN3D>(std::move(rtAnime));

        return rtGet<tinyRT_AN3D>(compPtr->pHandle);
    }

    // ---------- Runtime registry access (private) ----------

    // Access node by index, only for internal use
    tinyNodeRT* fromIndex(uint32_t index) {
        return nodes.get(nodeHandle(index));
    }

    const tinyNodeRT* fromIndex(uint32_t index) const {
        return nodes.get(nodeHandle(index));
    }

    template<typename T>
    tinyHandle rtAdd(T&& data) {
        return rtRegistry.add<T>(std::forward<T>(data)).handle;
    }


    template<typename T> struct DeferredRm : std::false_type {};
    template<> struct DeferredRm<tinyRT_SK3D> : std::true_type {};

    template<typename T>
    void rtRemove(const tinyHandle& handle) {
        if constexpr (DeferredRm<T>::value) rtRegistry.tQueueRm<T>(handle);
        else                                rtRegistry.tInstaRm<T>(handle);
    }
};
