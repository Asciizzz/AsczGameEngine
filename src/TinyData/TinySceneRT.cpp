#include "TinyData/TinySceneRT.hpp"

#include <stdexcept>
#include <thread>

template<typename T>
bool validIndex(TinyHandle handle, const std::vector<T>& vec) {
    return handle.valid() && handle.index < vec.size();
}



TinyHandle TinySceneRT::addRoot(const std::string& nodeName) {
    // Create a new root node
    TinyNodeRT rootNode(nodeName);
    rootNode.add<TinyNodeRT::T3D>();
    setRoot(nodes.add(std::move(rootNode)));

    return rootHandle();
}

TinyHandle TinySceneRT::addNode(const std::string& nodeName, TinyHandle parentHandle) {
    TinyNodeRT newNode(nodeName);
    newNode.add<TinyNodeRT::T3D>();

    if (!parentHandle.valid()) parentHandle = rootHandle();
    TinyNodeRT* parentNode = nodes.get(parentHandle);
    if (!parentNode) return TinyHandle();

    newNode.setParent(parentHandle);
    TinyHandle newNodeHandle = nodes.add(std::move(newNode));

    parentNode = nodes.get(parentHandle); // Re-fetch in case of invalidation
    parentNode->addChild(newNodeHandle);

    return newNodeHandle;
}

TinyHandle TinySceneRT::addNodeRaw(const std::string& nodeName) {
    TinyNodeRT newNode(nodeName);
    return nodes.add(std::move(newNode));
}

bool TinySceneRT::removeNode(TinyHandle nodeHandle, bool recursive) {
    TinyNodeRT* nodeToDelete = nodes.get(nodeHandle);
    if (!nodeToDelete || nodeHandle == rootHandle()) return false;

    std::vector<TinyHandle> childrenToDelete = nodeToDelete->childrenHandles;
    for (const TinyHandle& childHandle : childrenToDelete) {
        // If recursive, remove children, otherwise attach them to the deleted node's parent
        if (recursive) removeNode(childHandle, true);
        else reparentNode(childHandle, nodeToDelete->parentHandle);
    }

    // Remove this node from its parent's children list
    if (nodeToDelete->parentHandle.valid()) {
        TinyNodeRT* parentNode = nodes.get(nodeToDelete->parentHandle);
        if (parentNode) parentNode->removeChild(nodeHandle);
    }

    // Remove individual components runtime data
    removeComp<TinyNodeRT::T3D>(nodeHandle);
    removeComp<TinyNodeRT::MR3D>(nodeHandle);
    removeComp<TinyNodeRT::BA3D>(nodeHandle);
    removeComp<TinyNodeRT::SK3D>(nodeHandle);
    removeComp<TinyNodeRT::AN3D>(nodeHandle);
    nodes.instaRm(nodeHandle);

    return true;
}

bool TinySceneRT::flattenNode(TinyHandle nodeHandle) {
    return removeNode(nodeHandle, false);
}

bool TinySceneRT::reparentNode(TinyHandle nodeHandle, TinyHandle newParentHandle) {
    // Can't reparent root node or self
    if (nodeHandle == rootHandle() || nodeHandle == newParentHandle) {
        return false;
    }

    // Set parent to root if invalid
    if (!newParentHandle.valid()) newParentHandle = rootHandle();

    TinyNodeRT* nodeToMove = nodes.get(nodeHandle);
    TinyNodeRT* newParent = nodes.get(newParentHandle);
    if (!nodeToMove || !newParent) return false;

    // Check for cycles: make sure new parent is not a descendant of the node we're moving
    std::function<bool(TinyHandle)> isDescendant = [this, newParentHandle, &isDescendant](TinyHandle ancestor) -> bool {
        const TinyNodeRT* node = nodes.get(ancestor);
        if (!node) return false;
        
        for (const TinyHandle& childHandle : node->childrenHandles) {
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
    TinyNodeRT* currentParent = nodes.get(nodeToMove->parentHandle);
    if (currentParent) currentParent->removeChild(nodeHandle);

    // Add to new parent's children list
    newParent->addChild(nodeHandle);

    nodeToMove->setParent(newParentHandle);
    return true;
}

bool TinySceneRT::renameNode(TinyHandle nodeHandle, const std::string& newName) {
    TinyNodeRT* node = nodes.get(nodeHandle);
    if (!node) return false;

    node->name = newName;
    return true;
}



const TinyNodeRT* TinySceneRT::node(TinyHandle nodeHandle) const {
    return nodes.get(nodeHandle);
}

const std::vector<TinyNodeRT>& TinySceneRT::nodeView() const {
    return nodes.view();
}

bool TinySceneRT::nodeValid(TinyHandle nodeHandle) const {
    return nodes.valid(nodeHandle);
}

bool TinySceneRT::nodeOccupied(uint32_t index) const {
    return nodes.isOccupied(index);
}

TinyHandle TinySceneRT::nodeHandle(uint32_t index) const {
    return nodes.getHandle(index);
}

uint32_t TinySceneRT::nodeCount() const {
    return nodes.count();
}



TinyHandle TinySceneRT::nodeParent(TinyHandle nodeHandle) const {
    const TinyNodeRT* node = nodes.get(nodeHandle);
    return node ? node->parentHandle : TinyHandle();
}

std::vector<TinyHandle> TinySceneRT::nodeChildren(TinyHandle nodeHandle) const {
    const TinyNodeRT* node = nodes.get(nodeHandle);
    return node ? node->childrenHandles : std::vector<TinyHandle>();
}

bool TinySceneRT::setNodeParent(TinyHandle nodeHandle, TinyHandle newParentHandle) {
    TinyNodeRT* node = nodes.get(nodeHandle);
    if (!node || !nodes.valid(newParentHandle)) return false;

    node->setParent(newParentHandle);
    return true;
}

bool TinySceneRT::setNodeChildren(TinyHandle nodeHandle, const std::vector<TinyHandle>& newChildren) {
    TinyNodeRT* node = nodes.get(nodeHandle);
    if (!node) return false;

    // node->childrenHandles = newChildren;
    for (const TinyHandle& childHandle : newChildren) {
        if (nodes.valid(childHandle)) node->addChild(childHandle);
    }
    return true;
}




void TinySceneRT::addScene(const TinySceneRT* from, TinyHandle parentHandle) {
    if (!from || from->nodes.count() == 0) return;

    // Default to root node if no parent specified
    if (!parentHandle.valid()) parentHandle = rootHandle();

    // First pass: Add all nodes from 'from' scene as raw nodes

    // std::vector<TinyHandle> toHandles;
    UnorderedMap<uint32_t, TinyHandle> toHandleMap;

    const auto& from_items = from->nodeView();
    for (uint32_t i = 0; i < from_items.size(); ++i) {
        if (!from->nodeOccupied(i)) continue;

        const TinyNodeRT* fromNode = from->fromIndex(i);
        toHandleMap[i] = addNodeRaw(fromNode->name);
    }

    // Second pass: Construct nodes with proper remapped components

    for (uint32_t i = 0; i < from_items.size(); ++i) {
        TinyHandle fromHandle = from->nodeHandle(i);
        const TinyNodeRT* fromNode = from->node(fromHandle);
        if (!fromNode) continue;

        TinyHandle toHandle = toHandleMap[i];

        TinyNodeRT* toNode = nodes.get(toHandle);
        if (!toNode) continue; // Should not happen

        // Resolve parent-self relationships
        if (fromNode->parentHandle.valid()) {
            TinyHandle fromParentHandle = fromNode->parentHandle;
            toNode->setParent(toHandleMap[fromParentHandle.index]);
        } else { // <-- Root node in 'from' scene
            // Add child to parent
            TinyNodeRT* parentNode = nodes.get(parentHandle);

            if (parentNode) parentNode->addChild(toHandle);
            toNode->setParent(parentHandle);
        }

        // Resolve self-children relationships
        for (const TinyHandle& childHandle : fromNode->childrenHandles) {
            if (childHandle.index >= toHandleMap.size()) continue;
            toNode->addChild(toHandleMap[childHandle.index]);
        }

        // Resolve components

        if (fromNode->has<TinyNodeRT::T3D>()) {
            const auto* fromTransform = fromNode->get<TinyNodeRT::T3D>();
            auto* toTransform = writeComp<TinyNodeRT::T3D>(toHandle);
            *toTransform = *fromTransform;
        }

        if (fromNode->has<TinyNodeRT::MR3D>()) {
            const auto* fromMeshRender = fromNode->get<TinyNodeRT::MR3D>();
            auto* toMeshRender = writeComp<TinyNodeRT::MR3D>(toHandle);

            toMeshRender->pMeshHandle = fromMeshRender->pMeshHandle;

            // if (validIndex(fromMeshRender->skeleNodeHandle, from_items)) {
            //     toMeshRender->skeleNodeHandle = toHandleMap[fromMeshRender->skeleNodeHandle.index];
            // }

            if (toHandleMap.find(fromMeshRender->skeleNodeHandle.index) != toHandleMap.end()) {
                toMeshRender->skeleNodeHandle = toHandleMap[fromMeshRender->skeleNodeHandle.index];
            }
        }

        if (fromNode->has<TinyNodeRT::BA3D>()) {
            const auto* fromBoneAttach = fromNode->get<TinyNodeRT::BA3D>();
            auto* toBoneAttach = writeComp<TinyNodeRT::BA3D>(toHandle);

            if (toHandleMap.find(fromBoneAttach->skeleNodeHandle.index) != toHandleMap.end()) {
                toBoneAttach->skeleNodeHandle = toHandleMap[fromBoneAttach->skeleNodeHandle.index];
            }

            toBoneAttach->boneIndex = fromBoneAttach->boneIndex;
        }

        if (fromNode->has<TinyNodeRT::SK3D>()) {
            auto* toSkeleRT = writeComp<TinyNodeRT::SK3D>(toHandle);
            const auto* fromSkeleRT = from->rtComp<TinyNodeRT::SK3D>(fromHandle);
            toSkeleRT->copy(fromSkeleRT);
        }

        if (fromNode->has<TinyNodeRT::AN3D>()) {
            auto* toAnimeRT = writeComp<TinyNodeRT::AN3D>(toHandle);
            const auto* fromAnimeRT = from->rtComp<TinyNodeRT::AN3D>(fromHandle);

            *toAnimeRT = *fromAnimeRT;

            for (auto& anime : toAnimeRT->MAL()) {
                auto* toAnime = toAnimeRT->get(anime.second);

                // Remap each channel
                for (auto& channel : toAnime->channels) {
                    // if (validIndex(channel.node, toHandles)) {
                    //     channel.node = toHandles[channel.node.index];
                    // }
                    if (toHandleMap.find(channel.node.index) != toHandleMap.end()) {
                        channel.node = toHandleMap[channel.node.index];
                    }
                }
            }
        }
    }

    updateTransform(parentHandle); // Update transforms after adding new nodes
}





void TinySceneRT::updateRecursive(TinyHandle nodeHandle, const glm::mat4& parentGlobalTransform) {
    TinyHandle realHandle = nodeHandle.valid() ? nodeHandle : rootHandle();

    TinyNodeRT* node = nodes.get(realHandle);
    if (!node) return;

    // Update transform component
    TinyNodeRT::T3D* transform = rtComp<TinyNodeRT::T3D>(realHandle);
    glm::mat4 localMat = transform ? transform->local : glm::mat4(1.0f);

    // Update update local transform with bone attachment if applicable
    TinyNodeRT::BA3D* boneAttach = rtComp<TinyNodeRT::BA3D>(realHandle);
    glm::mat4 boneMat = glm::mat4(1.0f);
    if (boneAttach) {
        TinyHandle skeleNodeHandle = boneAttach->skeleNodeHandle;
        TinySkeletonRT* skeleRT = rtComp<TinyNodeRT::SK3D>(skeleNodeHandle);
        if (skeleRT) localMat = skeleRT->finalPose(boneAttach->boneIndex) * localMat;
    }

    // Update skeleton component
    TinySkeletonRT* rtSkele = rtComp<TinyNodeRT::SK3D>(realHandle);
    if (rtSkele) rtSkele->update();

    
    glm::mat4 transformMat = parentGlobalTransform * localMat;

    // Set global transform
    if (transform) transform->global = transformMat;

    // Recursively update all children
    for (const TinyHandle& childHandle : node->childrenHandles) {
        updateRecursive(childHandle, transformMat);
    }
}

void TinySceneRT::updateTransform(TinyHandle nodeHandle) {
    // Use root node if no valid handle provided
    TinyHandle realHandle = nodeHandle.valid() ? nodeHandle : rootHandle();

    TinyNodeRT* node = nodes.get(realHandle);
    if (!node) return;

    // Update everything recursively
    TinyNodeRT::T3D* parentTransform = rtComp<TinyNodeRT::T3D>(node->parentHandle);
    updateRecursive(realHandle, parentTransform ? parentTransform->global : glm::mat4(1.0f));
}


TinySkeletonRT* TinySceneRT::addSkeletonRT(TinyHandle nodeHandle) {
    TinyNodeRT::SK3D* compPtr = nodeComp<TinyNodeRT::SK3D>(nodeHandle);
    if (!compPtr) return nullptr; // Unable to add skeleton component (should not happen)

    // Create new empty valid runtime skeleton
    TinySkeletonRT rtSkele;
    rtSkele.init(sceneReq.deviceVK, sceneReq.fsRegistry, sceneReq.skinDescPool, sceneReq.skinDescLayout);
    // Repurpose pHandle into runtime skeleton handle
    compPtr->pSkeleHandle = rtAdd<TinySkeletonRT>(std::move(rtSkele));

    // Return the runtime skeleton
    return rtGet<TinySkeletonRT>(compPtr->pSkeleHandle);
}

TinyAnimeRT* TinySceneRT::addAnimationRT(TinyHandle nodeHandle) {
    TinyNodeRT::AN3D* compPtr = nodeComp<TinyNodeRT::AN3D>(nodeHandle);
    if (!compPtr) return nullptr; // Unable to add animation component (should not happen)

    TinyAnimeRT rtAnime;
    compPtr->pAnimeHandle = rtAdd<TinyAnimeRT>(std::move(rtAnime));

    return rtGet<TinyAnimeRT>(compPtr->pAnimeHandle);
}


// --------- Specific component's data access ---------

VkDescriptorSet TinySceneRT::nSkeleDescSet(TinyHandle nodeHandle) const {
    // Retrieve runtime skeleton data from TinyFS registry
    const TinySkeletonRT* rtSkele = rtComp<TinyNodeRT::SK3D>(nodeHandle);
    return rtSkele ? rtSkele->descSet() : VK_NULL_HANDLE;
}
