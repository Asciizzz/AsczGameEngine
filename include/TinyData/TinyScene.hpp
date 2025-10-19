#pragma once

#include "TinyData/TinyNode.hpp"
#include "TinyExt/TinyPool.hpp"

struct TinyScene {
    std::string name;
    TinyPool<TinyNode> nodes;
    TinyHandle rootHandle;

    TinyScene() = default;

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
};
