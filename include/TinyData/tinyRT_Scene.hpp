#pragma once

#include "tinyExt/tinyFS.hpp"

#include "tinyData/tinyRT_MeshRender3D.hpp"
#include "tinyData/tinyRT_Skeleton3D.hpp"
#include "tinyData/tinyRT_Anime3D.hpp"
#include "tinyData/tinyRT_Script.hpp"
#include "tinyData/tinyRT_Node.hpp"

#include "tinyData/tinySharedRes.hpp"

namespace tinyRT {

struct Scene {
private:

    // -------- Component to Runtime Type Mapping ---------

    /* The concept:

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
    
    // Type trait to map component types to their runtime equivalents
    template<typename T>
    struct RTResolver { using type = T; }; // Most types return themselves
    template<typename T> using RTResolver_t = typename RTResolver<T>::type;

    // Specializations for components that have separate runtime types
    template<> struct RTResolver<tinyNodeRT::SKEL3D> { using type = tinyRT_SKEL3D; };
    template<> struct RTResolver<tinyNodeRT::ANIM3D> { using type = tinyRT_ANIM3D; };
    template<> struct RTResolver<tinyNodeRT::MESHRD> { using type = tinyRT_MESHRD; };
    template<> struct RTResolver<tinyNodeRT::SCRIPT> { using type = tinyRT_SCRIPT; };

    // Helper to check if a type has a runtime equivalent (has pHandle)
    template<typename T>
    static constexpr bool no_resolver = std::is_same_v<T, RTResolver_t<T>>;

    #define IF_NO_RESOLVER(T) if constexpr (no_resolver<T>)
    #define IF_HAS_RESOLVER(T) if constexpr (!no_resolver<T>)
    #define IF_TYPE_EQ(A, B) if constexpr (type_eq<A, B>)

public:
    std::string name;

    Scene(const std::string& sceneName = "New Scene") : name(sceneName) {}

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    Scene(Scene&&) noexcept = default;
    Scene& operator=(Scene&&) noexcept = default;

    // --------- Core management ---------

    tinyHandle addRoot(const std::string& nodeName = "Root");
    void setRoot(tinyHandle handle) { rootHandle_ = handle; }
    tinyHandle rootHandle() const { return rootHandle_; }

    void setSharedRes(const tinySharedRes& sharedRes) { sharedRes_ = sharedRes; }
    const tinySharedRes& sharedRes() const { return sharedRes_; }

    // --------- Node management ---------

    // No add node by tinyNodeRT because of component logic
    tinyHandle addNode(const std::string& nodeName = "New Node", tinyHandle parentHandle = tinyHandle());
    tinyHandle addNodeRaw(const std::string& nodeName = "New Node");

    bool removeNode(tinyHandle nodeHandle, bool recursive = true);
    bool flattenNode(tinyHandle nodeHandle);
    bool reparentNode(tinyHandle nodeHandle, tinyHandle newParentHandle);
    bool renameNode(tinyHandle nodeHandle, const std::string& newName);

    const tinyNodeRT* node(tinyHandle nodeHandle) const;
    tinyHandle nodeHandle(uint32_t index) const;
    uint32_t nodeCount() const;

    tinyHandle nodeParent(tinyHandle nodeHandle) const;
    std::vector<tinyHandle> nodeChildren(tinyHandle nodeHandle) const;
    bool setNodeParent(tinyHandle nodeHandle, tinyHandle newParentHandle);
    bool setNodeChildren(tinyHandle nodeHandle, const std::vector<tinyHandle>& newChildren);

    // ------------------ Scene methods ------------------

    /* cleanse() explanation:
    Sort all nodes in a DFS manner starting from root node
    Ensuring scene instantiation (addScene) becomes easier and cleaner
    */

    bool isClean() const { return isClean_; }
    void cleanse();

    tinyHandle addScene(tinyHandle fromHandle, tinyHandle parentHandle = tinyHandle());

    // --------- Runtime registry access (public) ---------

    tinyRegistry& rtRegistry() noexcept { return rtRegistry_; }
    const tinyRegistry& rtRegistry() const noexcept { return rtRegistry_; }

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

    void rtFlushAllRms() {
        rtRegistry_.flushAllRms();
    }


    // -------- Component management --------- 

    struct NComp {
        tinyNodeRT::TRFM3D* trfm3D = nullptr;
        tinyNodeRT::BONE3D* bone3D = nullptr;
        tinyRT_MESHRD* meshRD = nullptr;
        tinyRT_SKEL3D* skel3D = nullptr;
        tinyRT_ANIM3D* anim3D = nullptr;
        tinyRT_SCRIPT* script = nullptr;
    };

    NComp nComp(tinyHandle nodeHandle) {
        tinyNodeRT* node = nodes_.get(nodeHandle);
        if (!node) return NComp();

        NComp comps;

        comps.trfm3D = rtComp<tinyNodeRT::TRFM3D>(nodeHandle);
        comps.bone3D = rtComp<tinyNodeRT::BONE3D>(nodeHandle);
        comps.meshRD = rtComp<tinyNodeRT::MESHRD>(nodeHandle);
        comps.skel3D = rtComp<tinyNodeRT::SKEL3D>(nodeHandle);
        comps.anim3D = rtComp<tinyNodeRT::ANIM3D>(nodeHandle);
        comps.script = rtComp<tinyNodeRT::SCRIPT>(nodeHandle);

        return comps;
    }

    const NComp nComp(tinyHandle nodeHandle) const {
        return const_cast<Scene*>(this)->nComp(nodeHandle);
    }

    // Retrieve runtime-resolved component pointer (return runtime component instead of node identity component)
    template<typename T>
    RTResolver_t<T>* rtComp(tinyHandle nodeHandle) {
        tinyNodeRT* node = nodes_.get(nodeHandle);
        if (!node || !node->has<T>()) return nullptr;

        T* compPtr = node->get<T>();

        if constexpr (no_resolver<T>) return compPtr;
        else return rtGet<RTResolver_t<T>>(compPtr->pHandle);
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

        IF_TYPE_EQ(T, tinyNodeRT::SKEL3D) return addSKEL3D_RT(compPtr); else
        IF_TYPE_EQ(T, tinyNodeRT::ANIM3D) return addANIM3D_RT(compPtr); else
        IF_TYPE_EQ(T, tinyNodeRT::MESHRD) return addMESHRD_RT(compPtr); else
        IF_TYPE_EQ(T, tinyNodeRT::SCRIPT) return addSCRIPT_RT(compPtr); else
        return compPtr;
    }

    template<typename T>
    bool removeComp(tinyHandle nodeHandle) {
        tinyNodeRT* node = nodes_.get(nodeHandle);
        if (!node || !node->has<T>()) return false;

        T* compPtr = node->get<T>();

        IF_HAS_RESOLVER(T) rtRemove<RTResolver_t<T>>(compPtr->pHandle);

        rmMap3D<T>(nodeHandle);

        return node->remove<T>();
    }

    // --------- Specific component's data access ---------

    template<typename T>
    const UnorderedMap<tinyHandle, tinyHandle>& mapRTRFM3D() const {
        IF_TYPE_EQ(T, tinyNodeRT::MESHRD) return mapMESHRD_; else
        IF_TYPE_EQ(T, tinyNodeRT::ANIM3D) return mapANIM3D_; else
        {
            static UnorderedMap<tinyHandle, tinyHandle> emptyMap;
            return emptyMap; // Empty map for unsupported types
        }
    }

    // -------------- Frame management --------------

    struct FStart {
        uint32_t frame;
        float dTime;
    };

    void setFStart(const FStart& fs) { fStart_ = fs; }
    void update();

private:
    bool isClean_ = false;

    FStart fStart_;

    tinySharedRes sharedRes_; // Shared resources and requirements
    tinyPool<tinyNodeRT> nodes_;
    tinyRegistry rtRegistry_; // Runtime registry for this scene

    tinyHandle rootHandle_;

    // ---------- Internal helpers ---------

    tinyNodeRT* nodeRef(tinyHandle nodeHandle) {
        return nodes_.get(nodeHandle);
    }

    template<typename T>
    T* nodeComp(tinyHandle nodeHandle) {
        tinyNodeRT* node = nodes_.get(nodeHandle);
        return node ? node->get<T>() : nullptr;
    }

    template<typename T>
    const tinyPool<T>& fsView() const {
        return sharedRes_.fsRegistry->view<T>();
    }

    // Cache of specific nodes_ for easy access

    tinyPool<tinyHandle> withMESHRD_;
    UnorderedMap<tinyHandle, tinyHandle> mapMESHRD_;

    tinyPool<tinyHandle> withANIM3D_;
    UnorderedMap<tinyHandle, tinyHandle> mapANIM3D_;

    tinyPool<tinyHandle> withSCRIPT_;
    UnorderedMap<tinyHandle, tinyHandle> mapSCRIPT_;

    // -------- General update ---------

    void updateRecursive(tinyHandle nodeHandle = tinyHandle(), const glm::mat4& parentGlobalTransform = glm::mat4(1.0f));

    // ---------- Runtime component management ----------

    template<typename T>
    void addMap3D(tinyHandle nodeHandle) {
        auto& mapInsert = [this](auto& map, auto& pool, tinyHandle handle) {
            map[handle] = pool.add(handle);
        };

        IF_TYPE_EQ(T, tinyNodeRT::MESHRD) mapInsert(mapMESHRD_, withMESHRD_, nodeHandle); else
        IF_TYPE_EQ(T, tinyNodeRT::ANIM3D) mapInsert(mapANIM3D_, withANIM3D_, nodeHandle); else
        IF_TYPE_EQ(T, tinyNodeRT::SCRIPT) mapInsert(mapSCRIPT_, withSCRIPT_, nodeHandle);
    }

    template<typename T>
    void rmMap3D(tinyHandle nodeHandle) {
        auto rmFromMapAndPool = [this](auto& map, auto& pool, tinyHandle handle) {
            auto it = map.find(handle);
            if (it == map.end()) return;

            pool.remove(it->second);
            map.erase(it);
        };

        IF_TYPE_EQ(T, tinyNodeRT::MESHRD) rmFromMapAndPool(mapMESHRD_, withMESHRD_, nodeHandle); else
        IF_TYPE_EQ(T, tinyNodeRT::ANIM3D) rmFromMapAndPool(mapANIM3D_, withANIM3D_, nodeHandle); else
        IF_TYPE_EQ(T, tinyNodeRT::SCRIPT) rmFromMapAndPool(mapSCRIPT_, withSCRIPT_, nodeHandle);
    }

    tinyRT_SKEL3D* addSKEL3D_RT(tinyNodeRT::SKEL3D* compPtr) {
        tinyRT_SKEL3D rtSKEL3D;
        rtSKEL3D.init(
            sharedRes_.deviceVk,
            &sharedRes_.fsView<tinySkeleton>(),
            sharedRes_.skinDescPool(),
            sharedRes_.skinDescLayout(),
            sharedRes_.maxFramesInFlight
        );

        compPtr->pHandle = rtAdd<tinyRT_SKEL3D>(std::move(rtSKEL3D));

        return rtGet<tinyRT_SKEL3D>(compPtr->pHandle);
    }

    tinyRT_ANIM3D* addANIM3D_RT(tinyNodeRT::ANIM3D* compPtr) {
        tinyRT_ANIM3D rtAnime;

        compPtr->pHandle = rtAdd<tinyRT_ANIM3D>(std::move(rtAnime));

        return rtGet<tinyRT_ANIM3D>(compPtr->pHandle);
    }

    tinyRT_MESHRD* addMESHRD_RT(tinyNodeRT::MESHRD* compPtr) {
        tinyRT_MESHRD rtMeshRT;
        rtMeshRT.init(
            sharedRes_.deviceVk,
            &sharedRes_.fsView<tinyMeshVk>(),
            sharedRes_.mrphWsDescLayout(),
            sharedRes_.mrphWsDescPool(),
            sharedRes_.maxFramesInFlight
        );

        compPtr->pHandle = rtAdd<tinyRT_MESHRD>(std::move(rtMeshRT));

        return rtGet<tinyRT_MESHRD>(compPtr->pHandle);
    }

    tinyRT_SCRIPT* addSCRIPT_RT(tinyNodeRT::SCRIPT* compPtr) {
        tinyRT_SCRIPT rtScript;
        rtScript.init(&sharedRes_.fsView<tinyScript>());

        compPtr->pHandle = rtAdd<tinyRT_SCRIPT>(std::move(rtScript));

        return rtGet<tinyRT_SCRIPT>(compPtr->pHandle);
    }

    // ---------- Runtime registry access (private) ----------

    // Access node by index, only for internal use
    tinyNodeRT* fromIndex(uint32_t index) {
        return nodes_.get(nodeHandle(index));
    }

    const tinyNodeRT*  fromIndex(uint32_t index) const {
        return nodes_.get(nodeHandle(index));
    }

    template<typename T>
    tinyHandle rtAdd(T&& data) {
        return rtRegistry_.add<T>(std::forward<T>(data)).handle;
    }

    // ---------- Scary removal function ---------- (spooky)

    template<typename T> struct DeferredRm : std::false_type {};
    template<> struct DeferredRm<tinyRT_SKEL3D> : std::true_type {}; // Skeleton descriptor set
    template<> struct DeferredRm<tinyRT_MESHRD> : std::true_type {}; // Morph weights descriptor set

    template<typename T>
    void rtRemove(const tinyHandle& handle) {
        if constexpr (DeferredRm<T>::value) rtRegistry_.tQueueRm<T>(handle);
        else                                rtRegistry_.tRemove<T>(handle);
    }
};

} // namespace tinyRT

using tinySceneRT = tinyRT::Scene;