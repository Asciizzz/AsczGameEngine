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
    nodes_.remove(nodeHandle);

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
            if (childHandle == newParentHandle) {
                return true; // Found cycle
            }
            if (isDescendant(childHandle)) {
                return true; // Found cycle in descendant
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

bool Scene::nodeValid(tinyHandle nodeHandle) const {
    return nodes_.valid(nodeHandle);
}

bool Scene::nodeOccupied(uint32_t index) const {
    return nodes_.isOccupied(index);
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




void Scene::addScene(const Scene* from, tinyHandle parentHandle) {
    if (!from || from->nodes_.count() == 0) return;

    // Default to root node if no parent specified
    if (!parentHandle.valid()) parentHandle = rootHandle();

    // First pass: Add all nodes_ from 'from' scene as raw nodes_

    // std::vector<tinyHandle> toHandles;
    UnorderedMap<uint32_t, tinyHandle> toHandleMap;

    const auto& from_items = from->nodes_.view();
    for (uint32_t i = 0; i < from_items.size(); ++i) {
        const tinyNodeRT* fromNode = from->fromIndex(i);
        if (!fromNode) continue;

        toHandleMap[i] = addNodeRaw(fromNode->name);
    }

    // Second pass: Construct nodes_ with proper remapped components

    for (uint32_t i = 0; i < from_items.size(); ++i) {
        tinyHandle fromHandle = from->nodeHandle(i);

        const tinyNodeRT* fromNode = from->node(fromHandle);
        if (!fromNode) continue;

        tinyHandle toHandle = toHandleMap[i];

        tinyNodeRT* toNode = nodes_.get(toHandle);
        if (!toNode) continue; // Should not happen

        // Establish parent-child relationships
        tinyHandle fromParentHandle = fromNode->parentHandle;
        tinyHandle toParentHandle = fromParentHandle.valid() ? toHandleMap[fromParentHandle.index] : parentHandle;

        toNode->setParent(toParentHandle);
        tinyNodeRT* parentNode = nodes_.get(toParentHandle);
        if (parentNode) parentNode->addChild(toHandle);

        // Resolve components

        if (fromNode->has<tinyNodeRT::TRFM3D>()) {
            const auto* fromTransform = fromNode->get<tinyNodeRT::TRFM3D>();
            auto* toTransform = writeComp<tinyNodeRT::TRFM3D>(toHandle);
            *toTransform = *fromTransform;
        }

        if (fromNode->has<tinyNodeRT::MESHRD>()) {
            // const auto* fromMeshRender = fromNode->get<tinyNodeRT::MESHRD>();
            const auto* fromMeshRender = from->rtComp<tinyNodeRT::MESHRD>(fromHandle);
            auto* toMeshRender = writeComp<tinyNodeRT::MESHRD>(toHandle);

            toMeshRender->setMesh(fromMeshRender->meshHandle());

            tinyHandle skeleNodeHandle = fromMeshRender->skeleNodeHandle();
            if (toHandleMap.find(skeleNodeHandle.index) != toHandleMap.end()) {
                toMeshRender->setSkeleNode(toHandleMap[skeleNodeHandle.index]);
            }
        }

        if (fromNode->has<tinyNodeRT::BONE3D>()) {
            const auto* fromBoneAttach = fromNode->get<tinyNodeRT::BONE3D>();
            auto* toBoneAttach = writeComp<tinyNodeRT::BONE3D>(toHandle);

            if (toHandleMap.find(fromBoneAttach->skeleNodeHandle.index) != toHandleMap.end()) {
                toBoneAttach->skeleNodeHandle = toHandleMap[fromBoneAttach->skeleNodeHandle.index];
            }

            toBoneAttach->boneIndex = fromBoneAttach->boneIndex;
        }

        if (fromNode->has<tinyNodeRT::SKEL3D>()) {
            auto* toSkeleRT = writeComp<tinyNodeRT::SKEL3D>(toHandle);
            const auto* fromSkeleRT = from->rtComp<tinyNodeRT::SKEL3D>(fromHandle);
            toSkeleRT->copy(fromSkeleRT);
        }

        if (fromNode->has<tinyNodeRT::ANIM3D>()) {
            auto* toAnimeRT = writeComp<tinyNodeRT::ANIM3D>(toHandle);
            const auto* fromAnimeRT = from->rtComp<tinyNodeRT::ANIM3D>(fromHandle);

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
    }
}





void Scene::updateRecursive(tinyHandle nodeHandle, const glm::mat4& parentGlobalTransform) {
    tinyHandle realHandle = nodeHandle.valid() ? nodeHandle : rootHandle();

    tinyNodeRT* node = nodes_.get(realHandle);
    if (!node) return;

    // Update transform component
    tinyNodeRT::TRFM3D* transform = rtComp<tinyNodeRT::TRFM3D>(realHandle);
    glm::mat4 localMat = transform ? transform->local : glm::mat4(1.0f);

    // Update update local transform with bone attachment if applicable
    tinyNodeRT::BONE3D* rtBONE3D = rtComp<tinyNodeRT::BONE3D>(realHandle);
    glm::mat4 boneMat = glm::mat4(1.0f);
    if (rtBONE3D) {
        tinyHandle skeleNodeHandle = rtBONE3D->skeleNodeHandle;
        tinyRT_SKEL3D* skeleRT = rtComp<tinyNodeRT::SKEL3D>(skeleNodeHandle);
        if (skeleRT) localMat = skeleRT->finalPose(rtBONE3D->boneIndex) * localMat;
    }

    // Update skeleton component
    tinyRT_SKEL3D* rtSKELE3D = rtComp<tinyNodeRT::SKEL3D>(realHandle);
    if (rtSKELE3D) {
        rtSKELE3D->update(0);
        rtSKELE3D->vkUpdate(curFrame_);
    }

    tinyRT_ANIM3D* rtANIM3D = rtComp<tinyNodeRT::ANIM3D>(realHandle);
    if (rtANIM3D) rtANIM3D->update(this, curDTime_);

    tinyRT_MESHRD* rtMESHRD = rtComp<tinyNodeRT::MESHRD>(realHandle);
    if (rtMESHRD) rtMESHRD->vkUpdate(curFrame_);

    tinyRT_SCRIPT* rtSCRIPT = rtComp<tinyNodeRT::SCRIPT>(realHandle);
    if (rtSCRIPT) rtSCRIPT->update(this, realHandle, curDTime_);

    glm::mat4 transformMat = parentGlobalTransform * localMat;
    if (transform) transform->global = transformMat;

    // Recursively update all children
    for (const tinyHandle& childHandle : node->childrenHandles) {
        updateRecursive(childHandle, transformMat);
    }
}

void Scene::update() {
    // Update everything recursively
    updateRecursive(rootHandle());
}

