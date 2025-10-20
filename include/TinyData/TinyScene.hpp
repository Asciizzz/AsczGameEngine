#pragma once

#include "TinyData/TinyNode.hpp"
#include "TinyExt/TinyRegistry.hpp"

struct TinyScene {
    std::string name;
    TinyPool<TinyNode> nodes;
    TinyHandle rootHandle;
    // Runtime registry data for node
    TinyRegistry rtRegistry;
        // For example, a node with runtime skeleton data

    TinyScene() = default;

    TinyScene(const TinyScene&) = delete;
    TinyScene& operator=(const TinyScene&) = delete;

    TinyScene(TinyScene&&) = default;
    TinyScene& operator=(TinyScene&&) = default;

    void updateGlbTransform(TinyHandle nodeHandle = TinyHandle(), const glm::mat4& parentGlobalTransform = glm::mat4(1.0f));

    TinyHandle addRoot(const std::string& nodeName = "Root");
    TinyHandle addNode(const std::string& nodeName = "New Node", TinyHandle parentHandle = TinyHandle());
    TinyHandle addNode(const TinyNode& nodeData, TinyHandle parentHandle = TinyHandle());

    void addScene(const TinyScene& scene, TinyHandle parentHandle = TinyHandle());
    bool removeNode(TinyHandle nodeHandle, bool recursive = true);
    bool flattenNode(TinyHandle nodeHandle);
    bool reparentNode(TinyHandle nodeHandle, TinyHandle newParentHandle);
    
    TinyNode* getNode(TinyHandle nodeHandle);
    const TinyNode* getNode(TinyHandle nodeHandle) const;
    bool renameNode(TinyHandle nodeHandle, const std::string& newName);

    // Runtime registry access

    template<typename T>
    TypeHandle addRT(T& data) {
        return rtRegistry.add<T>(std::move(data));
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

};