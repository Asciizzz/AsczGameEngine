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
    // Can't reparent root node or self
    if (nodeHandle == rootHandle() || nodeHandle == newParentHandle) {
        return false;
    }

    // Set parent to root if invalid
    if (!newParentHandle.valid()) newParentHandle = rootHandle();

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
    
    if (isDescendant(nodeHandle)) {
        return false; // Would create a cycle
    }
    
    // Remove from current parent's children list
    tinyNodeRT* currentParent = nodes_.get(nodeToMove->parentHandle);
    if (currentParent) currentParent->removeChild(nodeHandle);

    // Add to new parent's children list
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

tinyHandle Scene::nodeHandle(uint32_t index) const {
    return nodes_.getHandle(index);
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




void Scene::remapComponents(const UnorderedMap<tinyHandle, tinyHandle>& ogToNewMap) {
    // For each original to new handle mapping, remap components
    for (const auto& [ogHandle, newHandle] : ogToNewMap) {
        // Remap MESHRD
        if (rtComp<tinyNodeRT::MESHRD>(ogHandle)) {
            tinyRT_MESHRD* newMeshRD = writeComp<tinyNodeRT::MESHRD>(newHandle);
            const tinyRT_MESHRD* ogMeshRD = rtComp<tinyNodeRT::MESHRD>(ogHandle);

            newMeshRD->setMesh(ogMeshRD->meshHandle());

            tinyHandle ogSkeleNodeHandle = ogMeshRD->skeleNodeHandle();
            if (ogToNewMap.find(ogSkeleNodeHandle) != ogToNewMap.end()) {
                newMeshRD->setSkeleNode(ogToNewMap.at(ogSkeleNodeHandle));
            }
        }

        // Remap SKEL3D
        if (rtComp<tinyNodeRT::SKEL3D>(ogHandle)) {
            tinyRT_SKEL3D* newSkeleRT = writeComp<tinyNodeRT::SKEL3D>(newHandle);
            const tinyRT_SKEL3D* ogSkeleRT = rtComp<tinyNodeRT::SKEL3D>(ogHandle);

            newSkeleRT->copy(ogSkeleRT);
        }

        // Remap ANIM3D
        if (rtComp<tinyNodeRT::ANIM3D>(ogHandle)) {
            tinyRT_ANIM3D* newAnimeRT = writeComp<tinyNodeRT::ANIM3D>(newHandle);
            const tinyRT_ANIM3D* ogAnimeRT = rtComp<tinyNodeRT::ANIM3D>(ogHandle);

            *newAnimeRT = *ogAnimeRT;
        }

        // Remap SCRIPT
        if (rtComp<tinyNodeRT::SCRIPT>(ogHandle)) {
            tinyRT_SCRIPT* newScriptRT = writeComp<tinyNodeRT::SCRIPT>(newHandle);
            const tinyRT_SCRIPT* ogScriptRT = rtComp<tinyNodeRT::SCRIPT>(ogHandle);

            *newScriptRT = *ogScriptRT;
        }
    }
}




void Scene::cleanse() {
    tinyPool<tinyNodeRT> cleanedPool;
    UnorderedMap<tinyHandle, tinyHandle> ogToNewMap;

    /* First pass:
    + Recursively add to a new pool (Depth-First Search)
    + Establish parent-child relationships in the new pool
    */
    std::function<void(tinyHandle, tinyHandle)> recurse = [&](tinyHandle ogHandle, tinyHandle parent) {
        const tinyNodeRT* ogNode = nodes_.get(ogHandle);
        if (!ogNode) return;

        tinyNodeRT node = *ogNode; // Copy original node
        node.childrenHandles.clear(); // Clear children, will be re-added

        tinyHandle handle = cleanedPool.add(std::move(node));
        ogToNewMap[ogHandle] = handle;

        if (tinyNodeRT* newParent = cleanedPool.get(parent)) {
            newParent->addChild(handle);
        }

        std::vector<tinyHandle> ogChildren = ogNode->childrenHandles;

        for (const tinyHandle& childHandle : ogChildren) {
            recurse(childHandle, handle);
        }
    };

    /* Second pass: Remapping of components

    */

    recurse(rootHandle_, tinyHandle());

    nodes_.clear();
    nodes_ = std::move(cleanedPool);
    isClean_ = true;
}

tinyHandle Scene::addScene(tinyHandle fromHandle, tinyHandle parentHandle) {
    const Scene* from = sharedRes_.fsGet<Scene>(fromHandle);

    if (!from || from->nodes_.count() == 0) return tinyHandle();

    // Default to root node if no parent specified
    if (!parentHandle.valid()) parentHandle = rootHandle();

    // First pass: Add all nodes_ from 'from' scene as raw nodes_

    UnorderedMap<uint32_t, tinyHandle> toHandleMap;

    const auto& from_items = from->nodes_.view();
    for (uint32_t i = 0; i < from_items.size(); ++i) {
        const tinyNodeRT* fromNode = from->fromIndex(i);
        if (!fromNode) continue;

        toHandleMap[i] = addNodeRaw(fromNode->name);
    }

    // Second pass: Construct nodes_ with proper remapped components

    tinyHandle thisHandle = tinyHandle();

    for (auto& [i, toHandle] : toHandleMap) {
        tinyHandle fromHandle = from->nodeHandle(i);

        const tinyNodeRT* fromNode = from->node(fromHandle);
        if (!fromNode) continue;

        tinyHandle toHandle = toHandleMap[i];

        tinyNodeRT* toNode = nodes_.get(toHandle);
        if (!toNode) continue; // Should not happen

        // Establish parent-child relationships
        tinyHandle fromParentHandle = fromNode->parentHandle;

        tinyHandle toParentHandle;
        if (fromParentHandle.valid()) {
            toParentHandle = toHandleMap[fromParentHandle.index];
        } else {
            toParentHandle = parentHandle;
            thisHandle = toHandle; // This is the root node (which is also the added scene's root)
        }

        toNode->setParent(toParentHandle);
        tinyNodeRT* parentNode = nodes_.get(toParentHandle);
        if (parentNode) parentNode->addChild(toHandle);

        // Resolve components

        if (fromNode->has<tinyNodeRT::TRFM3D>()) {
            const auto* fromTransform = fromNode->get<tinyNodeRT::TRFM3D>();
            auto* toTransform = writeComp<tinyNodeRT::TRFM3D>(toHandle);
            *toTransform = *fromTransform;
        }

        if (const auto* fromMeshRender = from->rtComp<tinyNodeRT::MESHRD>(fromHandle)) {
            auto* toMeshRender = writeComp<tinyNodeRT::MESHRD>(toHandle);

            toMeshRender->setMesh(fromMeshRender->meshHandle());

            tinyHandle skeleNodeHandle = fromMeshRender->skeleNodeHandle();
            if (toHandleMap.find(skeleNodeHandle.index) != toHandleMap.end()) {
                toMeshRender->setSkeleNode(toHandleMap[skeleNodeHandle.index]);
            }
        }

        if (const auto* fromBoneAttach = fromNode->get<tinyNodeRT::BONE3D>()) {
            auto* toBoneAttach = writeComp<tinyNodeRT::BONE3D>(toHandle);

            if (toHandleMap.find(fromBoneAttach->skeleNodeHandle.index) != toHandleMap.end()) {
                toBoneAttach->skeleNodeHandle = toHandleMap[fromBoneAttach->skeleNodeHandle.index];
            }

            toBoneAttach->boneIndex = fromBoneAttach->boneIndex;
        }

        if (const auto* fromSkeleRT = from->rtComp<tinyNodeRT::SKEL3D>(fromHandle)) {
            auto* toSkeleRT = writeComp<tinyNodeRT::SKEL3D>(toHandle);
            toSkeleRT->copy(fromSkeleRT);
        }

        if (const auto* fromAnimeRT = from->rtComp<tinyNodeRT::ANIM3D>(fromHandle)) {
            auto* toAnimeRT = writeComp<tinyNodeRT::ANIM3D>(toHandle);

            *toAnimeRT = *fromAnimeRT;

            for (auto& anime : toAnimeRT->MAL()) {
                auto* toAnime = toAnimeRT->get(anime.second);

                // Remap each channel
                for (auto& channel : toAnime->channels) {
                    if (toHandleMap.find(channel.node.index) != toHandleMap.end()) {
                        channel.node = toHandleMap[channel.node.index];
                    }
                }
            }
        }

        if (const auto* fromScriptRT = from->rtComp<tinyNodeRT::SCRIPT>(fromHandle)) {
            auto* toScriptRT = writeComp<tinyNodeRT::SCRIPT>(toHandle);

            *toScriptRT = *fromScriptRT; // Allow copy

            // Remap node handles in script variables
            for (auto& [key, value] : toScriptRT->vMap()) {
                if (std::holds_alternative<typeHandle>(value)) {
                    typeHandle& th = std::get<typeHandle>(value);
                    
                    // Only remap if it's a node handle (type int)
                    if (th.isType<tinyNodeRT>() && th.handle.valid()) {
                        if (toHandleMap.find(th.handle.index) != toHandleMap.end()) {
                            th.handle = toHandleMap[th.handle.index];
                        }
                    }
                }
            }
        }
    }

    return thisHandle;
}





void Scene::updateRecursive(tinyHandle nodeHandle, const glm::mat4& parentGlobalTransform) {
    tinyHandle realHandle = nodeHandle.valid() ? nodeHandle : rootHandle();

    tinyNodeRT* node = nodes_.get(realHandle);
    if (!node) return;

    uint32_t curFrame_ = fStart_.frame;
    float curDTime_ = fStart_.dTime;

    // Script must be updated first before everything else
    tinyRT_SCRIPT* rtSCRIPT = rtComp<tinyNodeRT::SCRIPT>(realHandle);
    if (rtSCRIPT) rtSCRIPT->update(this, realHandle, curDTime_);

    tinyNodeRT::TRFM3D* transform = rtComp<tinyNodeRT::TRFM3D>(realHandle);
    glm::mat4 localMat = transform ? transform->local : glm::mat4(1.0f);

    // Update update local transform with bone attachment if available
    tinyNodeRT::BONE3D* rtBONE3D = rtComp<tinyNodeRT::BONE3D>(realHandle);
    if (rtBONE3D) {
        tinyHandle skeleNodeHandle = rtBONE3D->skeleNodeHandle;
        tinyRT_SKEL3D* skeleRT = rtComp<tinyNodeRT::SKEL3D>(skeleNodeHandle);
        if (skeleRT) localMat = skeleRT->finalPose(rtBONE3D->boneIndex) * localMat;
    }

    tinyRT_SKEL3D* rtSKELE3D = rtComp<tinyNodeRT::SKEL3D>(realHandle);
    if (rtSKELE3D) { rtSKELE3D->update(0); rtSKELE3D->vkUpdate(curFrame_); }

    tinyRT_ANIM3D* rtANIM3D = rtComp<tinyNodeRT::ANIM3D>(realHandle);
    if (rtANIM3D) rtANIM3D->update(this, curDTime_);

    tinyRT_MESHRD* rtMESHRD = rtComp<tinyNodeRT::MESHRD>(realHandle);
    if (rtMESHRD) rtMESHRD->vkUpdate(curFrame_);

    glm::mat4 transformMat = parentGlobalTransform * localMat;
    if (transform) transform->global = transformMat;

    // Recursively update all children
    for (const tinyHandle& childHandle : node->childrenHandles) {
        updateRecursive(childHandle, transformMat);
    }
}

void Scene::update() {
    updateRecursive(rootHandle());
}

