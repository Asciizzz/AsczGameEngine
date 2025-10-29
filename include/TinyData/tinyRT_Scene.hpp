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

    tinyNodeRT::SKEL3D has a tinyHandle pHandle that references the actual
    runtime Skeleton3D in the rtRegistry_, changing it would cause undefined
    behavior. So rtComp<tinyNodeRT::SKEL3D> will return the Skeleton3D* instead

    */
    template<typename T>
    struct RTResolver { using type = T; }; // Most type return themselves
    template<typename T> using RTResolver_t = typename RTResolver<T>::type;

    // Special types
    template<> struct RTResolver<tinyNodeRT::SKEL3D> { using type = tinyRT_SKEL3D; };
    template<> struct RTResolver<tinyNodeRT::ANIM3D> { using type = tinyRT_ANIM3D; };
    template<> struct RTResolver<tinyNodeRT::MESHRD> { using type = tinyRT_MESHR; };

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

        if constexpr (type_eq<T, tinyNodeRT::SKEL3D>) {
            return rtGet<tinyRT_SKEL3D>(compPtr->pHandle);
        } else if constexpr (type_eq<T, tinyNodeRT::ANIM3D>) {
            return rtGet<tinyRT_ANIM3D>(compPtr->pHandle);
        } else if constexpr (type_eq<T, tinyNodeRT::MESHRD>) {
            return rtGet<tinyRT_MESHR>(compPtr->pHandle);
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

        if constexpr (type_eq<T, tinyNodeRT::SKEL3D>) {
            return addSKEL3D_RT(compPtr);
        } else if constexpr (type_eq<T, tinyNodeRT::ANIM3D>) {
            return addANIM3D_RT(compPtr);
        } else if constexpr (type_eq<T, tinyNodeRT::MESHRD>) {
            return addMESHR_RT(compPtr);
        } else { // Other types return themselves
            return compPtr;
        }
    }

    template<typename T>
    bool removeComp(tinyHandle nodeHandle) {
        tinyNodeRT* node = nodes_.get(nodeHandle);
        if (!node || !node->has<T>()) return false;

        T* compPtr = node->get<T>();

        if constexpr (type_eq<T, tinyNodeRT::SKEL3D>) {
            rtRemove<tinyRT_SKEL3D>(compPtr->pHandle);
        } else if constexpr (type_eq<T, tinyNodeRT::ANIM3D>) {
            rtRemove<tinyRT_ANIM3D>(compPtr->pHandle);
        } else if constexpr (type_eq<T, tinyNodeRT::MESHRD>) {
            rtRemove<tinyRT_MESHR>(compPtr->pHandle);
        }

        rmMap3D<T>(nodeHandle);

        return node->remove<T>();
    }

    // --------- Specific component's data access ---------

    VkDescriptorSet nSkeleDescSet(tinyHandle nodeHandle) const {
        const tinyRT_SKEL3D* rtSkele = rtComp<tinyNodeRT::SKEL3D>(nodeHandle);
        return rtSkele ? rtSkele->descSet() : VK_NULL_HANDLE;
    }

    template<typename T>
    const UnorderedMap<tinyHandle, tinyHandle>& mapRTRFM3D() const {
        if constexpr (type_eq<T, tinyNodeRT::MESHRD>)      return mapMESHR_;
        else if constexpr (type_eq<T, tinyNodeRT::ANIM3D>) return mapANIM3D_;
        else {
            static UnorderedMap<tinyHandle, tinyHandle> emptyMap;
            return emptyMap; // Empty map for unsupported types
        }
    }

    template<typename T>
    const tinyPool<tinyHandle>& poolRTRFM3D() const {
        if constexpr (type_eq<T, tinyNodeRT::MESHRD>)      return withMESHR_;
        else if constexpr (type_eq<T, tinyNodeRT::ANIM3D>) return withANIM3D_;
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

    tinyPool<tinyHandle> withMESHR_;
    UnorderedMap<tinyHandle, tinyHandle> mapMESHR_;

    tinyPool<tinyHandle> withANIM3D_;
    UnorderedMap<tinyHandle, tinyHandle> mapANIM3D_;

    // -------- General update ---------

    void updateRecursive(tinyHandle nodeHandle = tinyHandle(), const glm::mat4& parentGlobalTransform = glm::mat4(1.0f));

    // ---------- Runtime component management ----------

    template<typename T>
    void addMap3D(tinyHandle nodeHandle) {
        auto& mapInsert = [this](auto& map, auto& pool, tinyHandle handle) {
            map[handle] = pool.add(handle);
        };

        if constexpr (type_eq<T, tinyNodeRT::MESHRD>)  {
            mapInsert(mapMESHR_, withMESHR_, nodeHandle);
        } else if constexpr (type_eq<T, tinyNodeRT::ANIM3D>) {
            mapInsert(mapANIM3D_, withANIM3D_, nodeHandle);
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

        if constexpr (type_eq<T, tinyNodeRT::MESHRD>) {
            rmFromMapAndPool(mapMESHR_, withMESHR_, nodeHandle);
        } else if constexpr (type_eq<T, tinyNodeRT::ANIM3D>) {
            rmFromMapAndPool(mapANIM3D_, withANIM3D_, nodeHandle);
        }
    }

    tinyRT_SKEL3D* addSKEL3D_RT(tinyNodeRT::SKEL3D* compPtr) {
        tinyRT_SKEL3D rtSKEL3D;
        rtSKEL3D.init(
            req_.deviceVk,
            req_.fsRegistry,
            req_.skinDescPool,
            req_.skinDescLayout,
            req_.maxFramesInFlight
        );

        // Repurpose pHandle to point to runtime skeleton
        compPtr->pHandle = rtAdd<tinyRT_SKEL3D>(std::move(rtSKEL3D  ));

        // Return the runtime skeleton
        return rtGet<tinyRT_SKEL3D>(compPtr->pHandle);
    }

    tinyRT_ANIM3D* addANIM3D_RT(tinyNodeRT::ANIM3D* compPtr) {
        tinyRT_ANIM3D rtAnime;
        compPtr->pHandle = rtAdd<tinyRT_ANIM3D>(std::move(rtAnime));

        return rtGet<tinyRT_ANIM3D>(compPtr->pHandle);
    }

    tinyRT_MESHR* addMESHR_RT(tinyNodeRT::MESHRD* compPtr) {
        tinyRT_MESHR rtMeshRT;
        rtMeshRT.init(req_.deviceVk, req_.fsRegistry);

        compPtr->pHandle = rtAdd<tinyRT_MESHR>(std::move(rtMeshRT));

        return rtGet<tinyRT_MESHR>(compPtr->pHandle);
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
    template<> struct DeferredRm<tinyRT_SKEL3D> : std::true_type {};

    template<typename T>
    void rtRemove(const tinyHandle& handle) {
        if constexpr (DeferredRm<T>::value) rtRegistry_.tQueueRm<T>(handle);
        else                                rtRegistry_.tInstaRm<T>(handle);
    }
};

} // namespace tinyRT

using tinySceneRT = tinyRT::Scene;