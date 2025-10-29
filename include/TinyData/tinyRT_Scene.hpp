#pragma once

#include "tinyExt/tinyFS.hpp"

#include "tinyData/tinyRT_MeshRender3D.hpp"
#include "tinyData/tinyRT_Skeleton3D.hpp"
#include "tinyData/tinyRT_Anime3D.hpp"
#include "tinyData/tinyRT_Node.hpp"

namespace tinyRT {

struct Scene {
private:

    // -------- writeComp's RTResolver ---------

    /* The idea:

    tinyNode's component themselves should not be modifiable at runtime, as 
    they directly reference the runtime data in rtRegistry_

    So instead, manipulations and retrievals of components should be done through
    the Scene's rtComp and writeComp functions, which will return the
    appropriate runtime component pointers, not the identity component pointers.

    For example:

    tinyNodeRT::SK3D has a tinyHandle pHandle that references the actual
    runtime Skeleton3D in the rtRegistry_, changing it would cause undefined
    behavior. So rtComp<tinyNodeRT::SK3D> will return the Skeleton3D* instead

    */
    template<typename T>
    struct RTResolver { using type = T; }; // Most type return themselves
    template<typename T> using RTResolver_t = typename RTResolver<T>::type;

    // Special types
    template<> struct RTResolver<tinyNodeRT::SK3D> { using type = tinyRT_SK3D; };
    template<> struct RTResolver<tinyNodeRT::AN3D> { using type = tinyRT_AN3D; };
    // template<> struct RTResolver<tinyNodeRT::MR3D> { using type = tinyRT_MR3D; };

public:
    struct Require {
        uint32_t maxFramesInFlight = 0; // If you messed this up the app just straight up jump off a cliff

        const tinyRegistry*   fsRegistry = nullptr; // For stuffs and things
        const tinyVk::Device* deviceVk = nullptr;   // For GPU resource creation

        VkDescriptorPool      skinDescPool   = VK_NULL_HANDLE;
        VkDescriptorSetLayout skinDescLayout = VK_NULL_HANDLE;

        bool valid() const {
            return  maxFramesInFlight > 0 &&
                    fsRegistry != nullptr &&
                    deviceVk != nullptr &&
                    skinDescPool   != VK_NULL_HANDLE &&
                    skinDescLayout != VK_NULL_HANDLE;
        }
    };

    std::string name;

    Scene(const std::string& sceneName = "New Scene") : name(sceneName) {}

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    Scene(Scene&&) = default;
    Scene& operator=(Scene&&) = default;

    // --------- Core management ---------

    tinyHandle addRoot(const std::string& nodeName = "Root");
    void setRoot(tinyHandle handle) { rootHandle_ = handle; }
    tinyHandle rootHandle() const { return rootHandle_; }

    void setSceneReq(const Require& req) {
        if (!req.valid()) throw std::invalid_argument("Invalid Require provided to Scene");
        req_ = req;
    }

    bool valid() const { return req_.valid(); }

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

    void addScene(const Scene* from, tinyHandle parentHandle = tinyHandle());

    // --------- Runtime registry access (public) ---------

    template<typename T>
    T* rtGet(const tinyHandle& handle) {
        return rtRegistry_.get<T>(handle);
    }

    template<typename T>
    const T* rtGet(const tinyHandle& handle) const {
        return rtRegistry_.get<T>(handle);
    }

    void* rtGet(const typeHandle& th) {
        return rtRegistry_.get(th);
    }

    template<typename T>
    T* rtGet(const typeHandle& th) {
        return rtRegistry_.get<T>(th);
    }

    template<typename T>
    const T* rtGet(const typeHandle& th) const {
        return rtRegistry_.get<T>(th);
    }

    template<typename T>
    bool rtTHasPendingRms() const {
        return rtRegistry_.tHasPendingRms<T>();
    }

    template<typename T>
    void rtTFlushAllRms() {
        rtRegistry_.tFlushAllRms<T>();
    }


    // -------- Component management --------- 

    // Retrieve runtime-resolved component pointer (return runtime component instead of node identity component)
    template<typename T>
    RTResolver_t<T>* rtComp(tinyHandle nodeHandle) {
        tinyNodeRT* node = nodes_.get(nodeHandle);
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
        return const_cast<Scene*>(this)->rtComp<T>(nodeHandle);
    }

    template<typename T>
    RTResolver_t<T>* writeComp(tinyHandle nodeHandle) {
        tinyNodeRT* node = nodes_.get(nodeHandle);
        if (!node) return nullptr;

        removeComp<T>(nodeHandle);
        T* compPtr = node->add<T>();

        addMap3D<T>(nodeHandle);

        if constexpr (type_eq<T, tinyNodeRT::SK3D>) {
            return addSK3D_RT(compPtr);
        } else if constexpr (type_eq<T, tinyNodeRT::AN3D>) {
            return addAN3D_RT(compPtr);
        } else if constexpr (type_eq<T, tinyNodeRT::MR3D>) { // In the future
            // return addMR3D_RT(compPtr);
            return compPtr; // Temporary
        } else { // Other types return themselves
            return compPtr;
        }
    }

    template<typename T>
    bool removeComp(tinyHandle nodeHandle) {
        tinyNodeRT* node = nodes_.get(nodeHandle);
        if (!node || !node->has<T>()) return false;

        T* compPtr = node->get<T>();

        if constexpr (type_eq<T, tinyNodeRT::SK3D>) {
            rtRemove<tinyRT_SK3D>(compPtr->pHandle);
        } else if constexpr (type_eq<T, tinyNodeRT::AN3D>) {
            rtRemove<tinyRT_AN3D>(compPtr->pHandle);
        } else if constexpr (type_eq<T, tinyNodeRT::MR3D>) {
            rtRemove<tinyRT_MR3D>(compPtr->pMeshHandle);
        }

        rmMap3D<T>(nodeHandle);

        return node->remove<T>();
    }

    // --------- Specific component's data access ---------

    VkDescriptorSet nSkeleDescSet(tinyHandle nodeHandle) const {
        const tinyRT_SK3D* rtSkele = rtComp<tinyNodeRT::SK3D>(nodeHandle);
        return rtSkele ? rtSkele->descSet() : VK_NULL_HANDLE;
    }

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

    void setFrame(uint32_t frame) { curFrame_ = frame; }
    void setDTime(float dTime) { curDTime_ = dTime; }
    void update();

private:
    Require req_;   // Scene requirements
    tinyPool<tinyNodeRT> nodes_;
    tinyRegistry rtRegistry_; // Runtime registry for this scene

    tinyHandle rootHandle_;
    uint32_t curFrame_ = 0;
    float curDTime_ = 0.0f;

    // ---------- Internal helpers ---------

    tinyNodeRT* nodeRef(tinyHandle nodeHandle) {
        return nodes_.get(nodeHandle);
    }

    template<typename T>
    T* nodeComp(tinyHandle nodeHandle) {
        tinyNodeRT* node = nodes_.get(nodeHandle);
        return node ? node->get<T>() : nullptr;
    }

    // Cache of specific nodes_ for easy access

    tinyPool<tinyHandle> withMR3D_;
    UnorderedMap<tinyHandle, tinyHandle> mapMR3D_;

    tinyPool<tinyHandle> withAN3D_;
    UnorderedMap<tinyHandle, tinyHandle> mapAN3D_;

    // -------- General update ---------

    void updateRecursive(tinyHandle nodeHandle = tinyHandle(), const glm::mat4& parentGlobalTransform = glm::mat4(1.0f));

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
        tinyRT_SK3D rtSK3D;
        rtSK3D.init(
            req_.deviceVk,
            req_.fsRegistry,
            req_.skinDescPool,
            req_.skinDescLayout,
            req_.maxFramesInFlight
        );

        // Repurpose pHandle to point to runtime skeleton
        compPtr->pHandle = rtAdd<tinyRT_SK3D>(std::move(rtSK3D  ));

        // Return the runtime skeleton
        return rtGet<tinyRT_SK3D>(compPtr->pHandle);
    }

    tinyRT_AN3D* addAN3D_RT(tinyNodeRT::AN3D* compPtr) {
        tinyRT_AN3D rtAnime;
        compPtr->pHandle = rtAdd<tinyRT_AN3D>(std::move(rtAnime));

        return rtGet<tinyRT_AN3D>(compPtr->pHandle);
    }

    tinyRT_MR3D* addMR3D_RT(tinyNodeRT::MR3D* compPtr) {
        tinyRT_MR3D rtMeshRT;
        // compPtr->pMeshHandle = rtAdd<tinyRT_MR3D>(std::move(rtMeshRT));

        return rtGet<tinyRT_MR3D>(compPtr->pMeshHandle);
    }

    // ---------- Runtime registry access (private) ----------

    // Access node by index, only for internal use
    tinyNodeRT* fromIndex(uint32_t index) {
        return nodes_.get(nodeHandle(index));
    }

    const tinyNodeRT* fromIndex(uint32_t index) const {
        return nodes_.get(nodeHandle(index));
    }

    template<typename T>
    tinyHandle rtAdd(T&& data) {
        return rtRegistry_.add<T>(std::forward<T>(data)).handle;
    }


    template<typename T> struct DeferredRm : std::false_type {};
    template<> struct DeferredRm<tinyRT_SK3D> : std::true_type {};

    template<typename T>
    void rtRemove(const tinyHandle& handle) {
        if constexpr (DeferredRm<T>::value) rtRegistry_.tQueueRm<T>(handle);
        else                                rtRegistry_.tInstaRm<T>(handle);
    }
};

} // namespace tinyRT

using tinySceneRT = tinyRT::Scene;