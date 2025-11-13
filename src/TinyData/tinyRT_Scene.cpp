#include "tinyData/tinyRT_Scene.hpp"

#include <stdexcept>
#include <thread>

using namespace tinyRT;

template<typename T>
bool validIndex(tinyHandle handle, const std::vector<T>& vec) {
    return handle.valid() && handle.index < vec.size();
}

// ----------------- Scene Management -----------------


tinyHandle Scene::addRoot(const std::string& nodeName) {
    // Create a new root node
    tinyNodeRT rootNode(nodeName);
    rootNode.add<tinyNodeRT::TRFM3D>();
    setRoot(nodes_.add(std::move(rootNode)));

    return rootHandle();
}

tinyHandle Scene::addNode(const std::string& nodeName, tinyHandle parentHandle) {
    tinyNodeRT newNode(nodeName);
    newNode.add<tinyNodeRT::TRFM3D>();

    if (!parentHandle.valid()) parentHandle = rootHandle();
    tinyNodeRT* parentNode = nodes_.get(parentHandle);
    if (!parentNode) return tinyHandle();

    newNode.setParent(parentHandle);
    tinyHandle newNodeHandle = nodes_.add(std::move(newNode));

    parentNode = nodes_.get(parentHandle); // Re-fetch in case of invalidation
    parentNode->addChild(newNodeHandle);

    isClean_ = false;
    return newNodeHandle;
}

tinyHandle Scene::addNodeRaw(const std::string& nodeName) {
    tinyNodeRT newNode(nodeName);
    return nodes_.add(std::move(newNode));
}

bool Scene::removeNode(tinyHandle nodeHandle, bool recursive) {
    tinyNodeRT* nodeToDelete = nodes_.get(nodeHandle);
    if (!nodeToDelete || nodeHandle == rootHandle()) return false;

    std::vector<tinyHandle> childrenToDelete = nodeToDelete->childrenHandles;
    for (const tinyHandle& childHandle : childrenToDelete) {
        // If recursive, remove children, otherwise attach them to the deleted node's parent
        if (recursive) removeNode(childHandle, true);
        else reparentNode(childHandle, nodeToDelete->parentHandle);
    }

    // Remove this node from its parent's children list
    if (nodeToDelete->parentHandle.valid()) {
        tinyNodeRT* parentNode = nodes_.get(nodeToDelete->parentHandle);
        if (parentNode) parentNode->removeChild(nodeHandle);
    }

    // Remove individual components runtime data
    removeComp<tinyNodeRT::TRFM3D>(nodeHandle);
    removeComp<tinyNodeRT::MESHRD>(nodeHandle);
    removeComp<tinyNodeRT::BONE3D>(nodeHandle);
    removeComp<tinyNodeRT::SKEL3D>(nodeHandle);
    removeComp<tinyNodeRT::ANIM3D>(nodeHandle);
    removeComp<tinyNodeRT::SCRIPT>(nodeHandle);
    nodes_.remove(nodeHandle);

    isClean_ = false;
    return true;
}

bool Scene::flattenNode(tinyHandle nodeHandle) {
    return removeNode(nodeHandle, false);
}

bool Scene::reparentNode(tinyHandle nodeHandle, tinyHandle newParentHandle) {
    if (!newParentHandle.valid()) newParentHandle = rootHandle();

    if (nodeHandle == rootHandle() || nodeHandle == newParentHandle) return false;

    tinyNodeRT* nodeToMove = nodes_.get(nodeHandle);
    tinyNodeRT* newParent = nodes_.get(newParentHandle);
    if (!nodeToMove || !newParent) return false;

    // Check for cycles: make sure new parent is not a descendant of the node we're moving
    std::function<bool(tinyHandle)> isDescendant = [this, newParentHandle, &isDescendant](tinyHandle ancestor) -> bool {
        const tinyNodeRT* node = nodes_.get(ancestor);
        if (!node) return false;
        
        for (const tinyHandle& childHandle : node->childrenHandles) {
            if (childHandle == newParentHandle || isDescendant(childHandle)) {
                return true; // Found cycle or is descendant
            }
        }
        return false;
    };
    
    if (isDescendant(nodeHandle)) return false; // Would create a cycle

    if (tinyNodeRT* currentParent = nodes_.get(nodeToMove->parentHandle)) {
        currentParent->removeChild(nodeHandle);
    }

    newParent->addChild(nodeHandle);
    nodeToMove->setParent(newParentHandle);

    isClean_ = false;
    return true;
}

bool Scene::renameNode(tinyHandle nodeHandle, const std::string& newName) {
    tinyNodeRT* node = nodes_.get(nodeHandle);
    if (!node) return false;

    node->name = newName;
    return true;
}



const tinyNodeRT* Scene::node(tinyHandle nodeHandle) const {
    return nodes_.get(nodeHandle);
}

uint32_t Scene::nodeCount() const {
    return nodes_.count();
}



tinyHandle Scene::nodeParent(tinyHandle nodeHandle) const {
    const tinyNodeRT* node = nodes_.get(nodeHandle);
    return node ? node->parentHandle : tinyHandle();
}

std::vector<tinyHandle> Scene::nodeChildren(tinyHandle nodeHandle) const {
    const tinyNodeRT* node = nodes_.get(nodeHandle);
    return node ? node->childrenHandles : std::vector<tinyHandle>();
}

bool Scene::setNodeParent(tinyHandle nodeHandle, tinyHandle newParentHandle) {
    tinyNodeRT* node = nodes_.get(nodeHandle);
    if (!node || !nodes_.valid(newParentHandle)) return false;

    node->setParent(newParentHandle);
    return true;
}

bool Scene::setNodeChildren(tinyHandle nodeHandle, const std::vector<tinyHandle>& newChildren) {
    tinyNodeRT* node = nodes_.get(nodeHandle);
    if (!node) return false;

    // node->childrenHandles = newChildren;
    for (const tinyHandle& childHandle : newChildren) {
        if (nodes_.valid(childHandle)) node->addChild(childHandle);
    }
    return true;
}



tinyHandle handleFromMap(UnorderedMap<tinyHandle, tinyHandle>& map, tinyHandle handle) {
    if (map.find(handle) == map.end()) return handle;
    return map[handle];
}

// Remap node handles in script variables
void remapHandlesInScriptMap(UnorderedMap<tinyHandle, tinyHandle>& from_to_map, tinyVarsMap& value_map) {
    for (auto& [key, value] : value_map) {
        if (!std::holds_alternative<typeHandle>(value)) continue;

        typeHandle& th = std::get<typeHandle>(value);
        if (!th.isType<tinyNodeRT>()) continue;

        th.handle = handleFromMap(from_to_map, th.handle);
    }
};

#include <iostream>
void Scene::cleanse() {
    tinyPool<tinyNodeRT> cleaned;
    UnorderedMap<tinyHandle, tinyHandle> old_new_map;

    /* First pass:
    + Recursively add to a new pool (Depth-First Search)
    + Establish parent-child relationships in the new pool
    */
    std::function<void(tinyHandle, tinyHandle)> recurse = [&](tinyHandle oldHandle, tinyHandle newParentHandle) {
        const tinyNodeRT* oldNode = nodes_.get(oldHandle);
        if (!oldNode) return;

        tinyNodeRT node = *oldNode;      // Copy original node
        node.setParent(newParentHandle); // Set new parent
        node.childrenHandles.clear();    // Clear children

        tinyHandle newHandle = cleaned.add(std::move(node));
        old_new_map[oldHandle] = newHandle;

        if (tinyNodeRT* newParent = cleaned.get(newParentHandle)) newParent->addChild(newHandle);
        else rootHandle_ = newHandle;

        for (const tinyHandle& childHandle : oldNode->childrenHandles) {
            recurse(childHandle, newHandle);
        }
    };

    recurse(rootHandle_, tinyHandle());

    /* Second pass: Remapping of EXISTING components */
    for (const auto& [oldHandle, newHandle] : old_new_map) {

        if (tinyRT_MESHRD* rtMESHRD = rtComp<tinyNodeRT::MESHRD>(oldHandle)) {
            rtMESHRD->setSkeleNode(handleFromMap(old_new_map, rtMESHRD->skeleNodeHandle()));
        }

        if (tinyNodeRT::BONE3D* rtBONE3D = rtComp<tinyNodeRT::BONE3D>(oldHandle)) {
            rtBONE3D->skeleNodeHandle = handleFromMap(old_new_map, rtBONE3D->skeleNodeHandle);
        }

        if (tinyRT_ANIM3D* rtANIM3D = rtComp<tinyNodeRT::ANIM3D>(oldHandle)) {
            for (auto& anime : rtANIM3D->MAL()) {
                auto* toAnime = rtANIM3D->get(anime.second);

                for (auto& channel : toAnime->channels) {
                    channel.node = handleFromMap(old_new_map, channel.node);
                }
            }
        }

        if (tinyRT_SCRIPT* rtSCRIPT = rtComp<tinyNodeRT::SCRIPT>(oldHandle)) {
            remapHandlesInScriptMap(old_new_map, rtSCRIPT->vMap());
            remapHandlesInScriptMap(old_new_map, rtSCRIPT->lMap());
        }
    }

    /* Third pass: Remap the cached handles */

    // Create a helper function to remap cached handles
    auto remapCachedHandles = [&](auto& map) {
        UnorderedMap<tinyHandle, tinyHandle> newMap;
        for (const auto& [oldHandle, rtHandle] : map) {
            tinyHandle newHandle = handleFromMap(old_new_map, oldHandle);
            newMap[newHandle] = rtHandle;
        }
        map = std::move(newMap);
    };

    remapCachedHandles(mapMESHRD_);
    remapCachedHandles(mapANIM3D_);
    remapCachedHandles(mapSCRIPT_);

    nodes_.clear();
    nodes_ = std::move(cleaned);
    isClean_ = true;
}

bool Scene::addScene(tinyHandle fromHandle, tinyHandle parentHandle) {
    const Scene* from = sharedRes_.fsGet<Scene>(fromHandle);

    if (!from || from->nodes_.count() == 0) return false;

    if (!parentHandle.valid()) parentHandle = rootHandle();

    // First pass: Add all nodes_ from 'from' scene as raw nodes_ recursively
    UnorderedMap<tinyHandle, tinyHandle> from_to_map;

    std::function<void(tinyHandle, tinyHandle)> recurseAdd = [&](tinyHandle fromHandle, tinyHandle toParentHandle) {
        const tinyNodeRT* fromNode = from->nodes_.get(fromHandle);
        if (!fromNode) return;

        tinyHandle toNodeHandle = addNodeRaw(fromNode->name);
        from_to_map[fromHandle] = toNodeHandle;

        // Establish parent-child relationships
        tinyNodeRT* toNode = nodes_.get(toNodeHandle);
        toNode->setParent(toParentHandle);

        if (tinyNodeRT* toParentNode = nodes_.get(toParentHandle)) {
            toParentNode->addChild(toNodeHandle);
        }

        for (const tinyHandle& fromChildHandle : fromNode->childrenHandles) {
            recurseAdd(fromChildHandle, toNodeHandle);
        }
    };

    recurseAdd(from->rootHandle(), parentHandle);

    // Second pass: Construct nodes_ with proper remapped components
    for (auto& [fromHandle, toHandle] : from_to_map) {
        CNWrap fromWrap = from->CWrap(fromHandle);

        if (fromWrap.trfm3D) {
            auto* toTransform = writeComp<tinyNodeRT::TRFM3D>(toHandle);
            *toTransform = *fromWrap.trfm3D;
        }

        if (fromWrap.meshRD) {
            auto* toMeshRender = writeComp<tinyNodeRT::MESHRD>(toHandle);

            toMeshRender->setMesh(fromWrap.meshRD->meshHandle());

            tinyHandle skeleNodeHandle = fromWrap.meshRD->skeleNodeHandle();
            toMeshRender->setSkeleNode(handleFromMap(from_to_map, skeleNodeHandle));
        }

        if (fromWrap.bone3D) {
            auto* toBoneAttach = writeComp<tinyNodeRT::BONE3D>(toHandle);

            toBoneAttach->skeleNodeHandle = handleFromMap(from_to_map, fromWrap.bone3D->skeleNodeHandle);
            toBoneAttach->boneIndex = fromWrap.bone3D->boneIndex;
        }

        if (fromWrap.skel3D) {
            auto* toSkeleRT = writeComp<tinyNodeRT::SKEL3D>(toHandle);
            toSkeleRT->copy(fromWrap.skel3D);
        }

        if (fromWrap.anim3D) {
            auto* toAnimeRT = writeComp<tinyNodeRT::ANIM3D>(toHandle);
            *toAnimeRT = *fromWrap.anim3D; // Allow copy

            for (auto& anime : toAnimeRT->MAL()) {
                auto* toAnime = toAnimeRT->get(anime.second);

                for (auto& channel : toAnime->channels) {
                    channel.node = handleFromMap(from_to_map, channel.node);
                }
            }
        }

        if (fromWrap.script) {
            auto* toScriptRT = writeComp<tinyNodeRT::SCRIPT>(toHandle);
            *toScriptRT = *fromWrap.script; // Allow copy

            remapHandlesInScriptMap(from_to_map, toScriptRT->vMap());
            remapHandlesInScriptMap(from_to_map, toScriptRT->lMap());
        }
    }

    return true;
}





void Scene::updateRecursive(tinyHandle nodeHandle, const glm::mat4& parentGlobalTransform) {
    tinyNodeRT* node = nodes_.get(nodeHandle);
    if (!node) return;

    uint32_t curFrame_ = fStart_.frame;
    float curDTime_ = fStart_.deltaTime;

    NWrap comps = Wrap(nodeHandle);

    // Script must be updated first before everything else
    if (comps.script) comps.script->update(this, nodeHandle, curDTime_);

    glm::mat4 localMat = comps.trfm3D ? comps.trfm3D->local : glm::mat4(1.0f);

    // Update update local transform with bone attachment if available
    if (comps.bone3D) {
        tinyHandle skeleNodeHandle = comps.bone3D->skeleNodeHandle;
        tinyRT_SKEL3D* skeleRT = rtComp<tinyNodeRT::SKEL3D>(skeleNodeHandle);

        if (skeleRT) localMat = skeleRT->finalPose(comps.bone3D->boneIndex) * localMat;
    }

    if (comps.skel3D) { comps.skel3D->update(0); comps.skel3D->vkUpdate(curFrame_); }

    if (comps.anim3D) comps.anim3D->update(this, curDTime_);

    if (comps.meshRD) comps.meshRD->vkUpdate(curFrame_);

    glm::mat4 transformMat = parentGlobalTransform * localMat;
    if (comps.trfm3D) comps.trfm3D->global = transformMat;

    for (const tinyHandle& childHandle : node->childrenHandles) {
        updateRecursive(childHandle, transformMat);
    }
}

void Scene::update() {
    updateRecursive(rootHandle());
}

