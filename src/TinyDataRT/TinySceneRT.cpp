#include "tinyDataRT/tinySceneRT.hpp"

#include <stdexcept>
#include <thread>

template<typename T>
bool validIndex(tinyHandle handle, const std::vector<T>& vec) {
    return handle.valid() && handle.index < vec.size();
}



tinyHandle tinySceneRT::addRoot(const std::string& nodeName) {
    // Create a new root node
    tinyNodeRT rootNode(nodeName);
    rootNode.add<tinyNodeRT::T3D>();
    setRoot(nodes.add(std::move(rootNode)));

    return rootHandle();
}

tinyHandle tinySceneRT::addNode(const std::string& nodeName, tinyHandle parentHandle) {
    tinyNodeRT newNode(nodeName);
    newNode.add<tinyNodeRT::T3D>();

    if (!parentHandle.valid()) parentHandle = rootHandle();
    tinyNodeRT* parentNode = nodes.get(parentHandle);
    if (!parentNode) return tinyHandle();

    newNode.setParent(parentHandle);
    tinyHandle newNodeHandle = nodes.add(std::move(newNode));

    parentNode = nodes.get(parentHandle); // Re-fetch in case of invalidation
    parentNode->addChild(newNodeHandle);

    return newNodeHandle;
}

tinyHandle tinySceneRT::addNodeRaw(const std::string& nodeName) {
    tinyNodeRT newNode(nodeName);
    return nodes.add(std::move(newNode));
}

bool tinySceneRT::removeNode(tinyHandle nodeHandle, bool recursive) {
    tinyNodeRT* nodeToDelete = nodes.get(nodeHandle);
    if (!nodeToDelete || nodeHandle == rootHandle()) return false;

    std::vector<tinyHandle> childrenToDelete = nodeToDelete->childrenHandles;
    for (const tinyHandle& childHandle : childrenToDelete) {
        // If recursive, remove children, otherwise attach them to the deleted node's parent
        if (recursive) removeNode(childHandle, true);
        else reparentNode(childHandle, nodeToDelete->parentHandle);
    }

    // Remove this node from its parent's children list
    if (nodeToDelete->parentHandle.valid()) {
        tinyNodeRT* parentNode = nodes.get(nodeToDelete->parentHandle);
        if (parentNode) parentNode->removeChild(nodeHandle);
    }

    // Remove individual components runtime data
    removeComp<tinyNodeRT::T3D>(nodeHandle);
    removeComp<tinyNodeRT::MR3D>(nodeHandle);
    removeComp<tinyNodeRT::BA3D>(nodeHandle);
    removeComp<tinyNodeRT::SK3D>(nodeHandle);
    removeComp<tinyNodeRT::AN3D>(nodeHandle);
    nodes.instaRm(nodeHandle);

    return true;
}

bool tinySceneRT::flattenNode(tinyHandle nodeHandle) {
    return removeNode(nodeHandle, false);
}

bool tinySceneRT::reparentNode(tinyHandle nodeHandle, tinyHandle newParentHandle) {
    // Can't reparent root node or self
    if (nodeHandle == rootHandle() || nodeHandle == newParentHandle) {
        return false;
    }

    // Set parent to root if invalid
    if (!newParentHandle.valid()) newParentHandle = rootHandle();

    tinyNodeRT* nodeToMove = nodes.get(nodeHandle);
    tinyNodeRT* newParent = nodes.get(newParentHandle);
    if (!nodeToMove || !newParent) return false;

    // Check for cycles: make sure new parent is not a descendant of the node we're moving
    std::function<bool(tinyHandle)> isDescendant = [this, newParentHandle, &isDescendant](tinyHandle ancestor) -> bool {
        const tinyNodeRT* node = nodes.get(ancestor);
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
    tinyNodeRT* currentParent = nodes.get(nodeToMove->parentHandle);
    if (currentParent) currentParent->removeChild(nodeHandle);

    // Add to new parent's children list
    newParent->addChild(nodeHandle);

    nodeToMove->setParent(newParentHandle);
    return true;
}

bool tinySceneRT::renameNode(tinyHandle nodeHandle, const std::string& newName) {
    tinyNodeRT* node = nodes.get(nodeHandle);
    if (!node) return false;

    node->name = newName;
    return true;
}



const tinyNodeRT* tinySceneRT::node(tinyHandle nodeHandle) const {
    return nodes.get(nodeHandle);
}

const std::vector<tinyNodeRT>& tinySceneRT::nodeView() const {
    return nodes.view();
}

bool tinySceneRT::nodeValid(tinyHandle nodeHandle) const {
    return nodes.valid(nodeHandle);
}

bool tinySceneRT::nodeOccupied(uint32_t index) const {
    return nodes.isOccupied(index);
}

tinyHandle tinySceneRT::nodeHandle(uint32_t index) const {
    return nodes.getHandle(index);
}

uint32_t tinySceneRT::nodeCount() const {
    return nodes.count();
}



tinyHandle tinySceneRT::nodeParent(tinyHandle nodeHandle) const {
    const tinyNodeRT* node = nodes.get(nodeHandle);
    return node ? node->parentHandle : tinyHandle();
}

std::vector<tinyHandle> tinySceneRT::nodeChildren(tinyHandle nodeHandle) const {
    const tinyNodeRT* node = nodes.get(nodeHandle);
    return node ? node->childrenHandles : std::vector<tinyHandle>();
}

bool tinySceneRT::setNodeParent(tinyHandle nodeHandle, tinyHandle newParentHandle) {
    tinyNodeRT* node = nodes.get(nodeHandle);
    if (!node || !nodes.valid(newParentHandle)) return false;

    node->setParent(newParentHandle);
    return true;
}

bool tinySceneRT::setNodeChildren(tinyHandle nodeHandle, const std::vector<tinyHandle>& newChildren) {
    tinyNodeRT* node = nodes.get(nodeHandle);
    if (!node) return false;

    // node->childrenHandles = newChildren;
    for (const tinyHandle& childHandle : newChildren) {
        if (nodes.valid(childHandle)) node->addChild(childHandle);
    }
    return true;
}




void tinySceneRT::addScene(const tinySceneRT* from, tinyHandle parentHandle) {
    if (!from || from->nodes.count() == 0) return;

    // Default to root node if no parent specified
    if (!parentHandle.valid()) parentHandle = rootHandle();

    // First pass: Add all nodes from 'from' scene as raw nodes

    // std::vector<tinyHandle> toHandles;
    UnorderedMap<uint32_t, tinyHandle> toHandleMap;

    const auto& from_items = from->nodeView();
    for (uint32_t i = 0; i < from_items.size(); ++i) {
        if (!from->nodeOccupied(i)) continue;

        const tinyNodeRT* fromNode = from->fromIndex(i);
        toHandleMap[i] = addNodeRaw(fromNode->name);
    }

    // Second pass: Construct nodes with proper remapped components

    for (uint32_t i = 0; i < from_items.size(); ++i) {
        tinyHandle fromHandle = from->nodeHandle(i);
        const tinyNodeRT* fromNode = from->node(fromHandle);
        if (!fromNode) continue;

        tinyHandle toHandle = toHandleMap[i];

        tinyNodeRT* toNode = nodes.get(toHandle);
        if (!toNode) continue; // Should not happen

        // Resolve parent-self relationships
        if (fromNode->parentHandle.valid()) {
            tinyHandle fromParentHandle = fromNode->parentHandle;
            toNode->setParent(toHandleMap[fromParentHandle.index]);
        } else { // <-- Root node in 'from' scene
            // Add child to parent
            tinyNodeRT* parentNode = nodes.get(parentHandle);

            if (parentNode) parentNode->addChild(toHandle);
            toNode->setParent(parentHandle);
        }

        // Resolve self-children relationships
        for (const tinyHandle& childHandle : fromNode->childrenHandles) {
            if (childHandle.index >= toHandleMap.size()) continue;
            toNode->addChild(toHandleMap[childHandle.index]);
        }

        // Resolve components

        if (fromNode->has<tinyNodeRT::T3D>()) {
            const auto* fromTransform = fromNode->get<tinyNodeRT::T3D>();
            auto* toTransform = writeComp<tinyNodeRT::T3D>(toHandle);
            *toTransform = *fromTransform;
        }

        if (fromNode->has<tinyNodeRT::MR3D>()) {
            const auto* fromMeshRender = fromNode->get<tinyNodeRT::MR3D>();
            auto* toMeshRender = writeComp<tinyNodeRT::MR3D>(toHandle);

            toMeshRender->pMeshHandle = fromMeshRender->pMeshHandle;

            if (toHandleMap.find(fromMeshRender->skeleNodeHandle.index) != toHandleMap.end()) {
                toMeshRender->skeleNodeHandle = toHandleMap[fromMeshRender->skeleNodeHandle.index];
            }
        }

        if (fromNode->has<tinyNodeRT::BA3D>()) {
            const auto* fromBoneAttach = fromNode->get<tinyNodeRT::BA3D>();
            auto* toBoneAttach = writeComp<tinyNodeRT::BA3D>(toHandle);

            if (toHandleMap.find(fromBoneAttach->skeleNodeHandle.index) != toHandleMap.end()) {
                toBoneAttach->skeleNodeHandle = toHandleMap[fromBoneAttach->skeleNodeHandle.index];
            }

            toBoneAttach->boneIndex = fromBoneAttach->boneIndex;
        }

        if (fromNode->has<tinyNodeRT::SK3D>()) {
            printf("\033[33mCopying tinyRT_SK3D runtime skeleton...\033[0m\n");
            auto* toSkeleRT = writeComp<tinyNodeRT::SK3D>(toHandle);
            printf("tinyRT_SK3D wrote comp\n");
            const auto* fromSkeleRT = from->rtComp<tinyNodeRT::SK3D>(fromHandle);
            toSkeleRT->copy(fromSkeleRT);
            printf("tinyRT_SK3D copied!\n");
        }

        if (fromNode->has<tinyNodeRT::AN3D>()) {
            auto* toAnimeRT = writeComp<tinyNodeRT::AN3D>(toHandle);
            const auto* fromAnimeRT = from->rtComp<tinyNodeRT::AN3D>(fromHandle);

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





void tinySceneRT::updateRecursive(tinyHandle nodeHandle, const glm::mat4& parentGlobalTransform) {
    tinyHandle realHandle = nodeHandle.valid() ? nodeHandle : rootHandle();

    tinyNodeRT* node = nodes.get(realHandle);
    if (!node) return;

    // Update transform component
    tinyNodeRT::T3D* transform = rtComp<tinyNodeRT::T3D>(realHandle);
    glm::mat4 localMat = transform ? transform->local : glm::mat4(1.0f);

    // Update update local transform with bone attachment if applicable
    tinyNodeRT::BA3D* boneAttach = rtComp<tinyNodeRT::BA3D>(realHandle);
    glm::mat4 boneMat = glm::mat4(1.0f);
    if (boneAttach) {
        tinyHandle skeleNodeHandle = boneAttach->skeleNodeHandle;
        tinyRT_SK3D* skeleRT = rtComp<tinyNodeRT::SK3D>(skeleNodeHandle);
        if (skeleRT) localMat = skeleRT->finalPose(boneAttach->boneIndex) * localMat;
    }

    // Update skeleton component
    tinyRT_SK3D* rtSkele = rtComp<tinyNodeRT::SK3D>(realHandle);
    if (rtSkele) rtSkele->update(0, curFrame_);

    // Update animation component
    tinyRT_AN3D* rtAnime = rtComp<tinyNodeRT::AN3D>(realHandle);
    if (rtAnime) rtAnime->update(this, curDTime_);

    glm::mat4 transformMat = parentGlobalTransform * localMat;

    // Set global transform
    if (transform) transform->global = transformMat;

    // Recursively update all children
    for (const tinyHandle& childHandle : node->childrenHandles) {
        updateRecursive(childHandle, transformMat);
    }
}

void tinySceneRT::update() {
    // Update everything recursively
    updateRecursive(rootHandle());
}

