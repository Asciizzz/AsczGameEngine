#include "TinyData/TinyScene.hpp"

#include <stdexcept>
#include <thread>

template<typename T>
bool validIndex(TinyHandle handle, const std::vector<T>& vec) {
    return handle.valid() && handle.index < vec.size();
}



TinyHandle TinyScene::addRoot(const std::string& nodeName) {
    // Create a new root node
    TinyNode rootNode(nodeName);
    rootNode.add<TinyNode::Transform>();
    setRoot(nodes.add(std::move(rootNode)));

    return rootHandle();
}

TinyHandle TinyScene::addNode(const std::string& nodeName, TinyHandle parentHandle) {
    TinyNode newNode(nodeName);
    newNode.add<TinyNode::Transform>();

    if (!parentHandle.valid()) parentHandle = rootHandle();
    TinyNode* parentNode = nodes.get(parentHandle);
    if (!parentNode) return TinyHandle();

    newNode.setParent(parentHandle);
    TinyHandle newNodeHandle = nodes.add(std::move(newNode));

    parentNode = nodes.get(parentHandle); // Re-fetch in case of invalidation
    parentNode->addChild(newNodeHandle);

    return newNodeHandle;
}

TinyHandle TinyScene::addNodeRaw(const std::string& nodeName) {
    TinyNode newNode(nodeName);
    return nodes.add(std::move(newNode));
}

bool TinyScene::removeNode(TinyHandle nodeHandle, bool recursive) {
    TinyNode* nodeToDelete = nodes.get(nodeHandle);
    if (!nodeToDelete || nodeHandle == rootHandle()) return false;

    std::vector<TinyHandle> childrenToDelete = nodeToDelete->childrenHandles;
    for (const TinyHandle& childHandle : childrenToDelete) {
        // If recursive, remove children, otherwise attach them to the deleted node's parent
        if (recursive) removeNode(childHandle, true);
        else reparentNode(childHandle, nodeToDelete->parentHandle);
    }

    // Remove this node from its parent's children list
    if (nodeToDelete->parentHandle.valid()) {
        TinyNode* parentNode = nodes.get(nodeToDelete->parentHandle);
        if (parentNode) parentNode->removeChild(nodeHandle);
    }

    removeComp<TinyNode::Transform>(nodeHandle);
    removeComp<TinyNode::MeshRender>(nodeHandle);
    removeComp<TinyNode::BoneAttach>(nodeHandle);
    removeComp<TinyNode::Skeleton>(nodeHandle);
    removeComp<TinyNode::Animation>(nodeHandle);

    nodes.remove(nodeHandle);

    return true;
}

bool TinyScene::flattenNode(TinyHandle nodeHandle) {
    return removeNode(nodeHandle, false);
}

bool TinyScene::reparentNode(TinyHandle nodeHandle, TinyHandle newParentHandle) {
    // Can't reparent root node or self
    if (nodeHandle == rootHandle() || nodeHandle == newParentHandle) {
        return false;
    }

    // Set parent to root if invalid
    if (!newParentHandle.valid()) newParentHandle = rootHandle();

    TinyNode* nodeToMove = nodes.get(nodeHandle);
    TinyNode* newParent = nodes.get(newParentHandle);
    if (!nodeToMove || !newParent) return false;

    // Check for cycles: make sure new parent is not a descendant of the node we're moving
    std::function<bool(TinyHandle)> isDescendant = [this, newParentHandle, &isDescendant](TinyHandle ancestor) -> bool {
        const TinyNode* node = nodes.get(ancestor);
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
    TinyNode* currentParent = nodes.get(nodeToMove->parentHandle);
    if (currentParent) currentParent->removeChild(nodeHandle);

    // Add to new parent's children list
    newParent->addChild(nodeHandle);

    nodeToMove->setParent(newParentHandle);
    return true;
}

bool TinyScene::renameNode(TinyHandle nodeHandle, const std::string& newName) {
    TinyNode* node = nodes.get(nodeHandle);
    if (!node) return false;

    node->name = newName;
    return true;
}




const TinyNode* TinyScene::node(TinyHandle nodeHandle) const {
    return nodes.get(nodeHandle);
}

const std::vector<TinyNode>& TinyScene::nodeView() const {
    return nodes.view();
}

bool TinyScene::nodeValid(TinyHandle nodeHandle) const {
    return nodes.valid(nodeHandle);
}

bool TinyScene::nodeOccupied(uint32_t index) const {
    return nodes.isOccupied(index);
}

TinyHandle TinyScene::nodeHandle(uint32_t index) const {
    return nodes.getHandle(index);
}

uint32_t TinyScene::nodeCount() const {
    return nodes.count();
}



TinyHandle TinyScene::nodeParent(TinyHandle nodeHandle) const {
    const TinyNode* node = nodes.get(nodeHandle);
    return node ? node->parentHandle : TinyHandle();
}

std::vector<TinyHandle> TinyScene::nodeChildren(TinyHandle nodeHandle) const {
    const TinyNode* node = nodes.get(nodeHandle);
    return node ? node->childrenHandles : std::vector<TinyHandle>();
}

bool TinyScene::setNodeParent(TinyHandle nodeHandle, TinyHandle newParentHandle) {
    TinyNode* node = nodes.get(nodeHandle);
    if (!node || !nodes.valid(newParentHandle)) return false;

    node->setParent(newParentHandle);
    return true;
}

bool TinyScene::setNodeChildren(TinyHandle nodeHandle, const std::vector<TinyHandle>& newChildren) {
    TinyNode* node = nodes.get(nodeHandle);
    if (!node) return false;

    // node->childrenHandles = newChildren;
    for (const TinyHandle& childHandle : newChildren) {
        if (nodes.valid(childHandle)) node->addChild(childHandle);
    }
    return true;
}




void TinyScene::addScene(const TinyScene* from, TinyHandle parentHandle) {
    if (!from || from->nodes.count() == 0) return;

    // Default to root node if no parent specified
    if (!parentHandle.valid()) parentHandle = rootHandle();

    // First pass: Add all nodes from 'from' scene as raw nodes

    std::vector<TinyHandle> toHandles;
    const auto& from_items = from->nodeView();
    for (uint32_t i = 0; i < from_items.size(); ++i) {
        if (!from->nodeOccupied(i)) continue;

        const TinyNode* fromNode = from->fromIndex(i);
        toHandles.push_back(addNodeRaw(fromNode->name));
    }

    // Second pass: Construct nodes with proper remapped components

    for (uint32_t i = 0; i < from_items.size(); ++i) {
        TinyHandle fromHandle = from->nodeHandle(i);
        const TinyNode* fromNode = from->node(fromHandle);
        if (!fromNode) continue;

        TinyHandle toHandle = toHandles[i];

        TinyNode* toNode = nodes.get(toHandle);
        if (!toNode) continue; // Should not happen

        // Resolve parent-self relationships
        if (fromNode->parentHandle.valid()) {
            toNode->setParent(toHandles[fromNode->parentHandle.index]);
        } else { // <-- Root node in 'from' scene
            // Add child to parent
            TinyNode* parentNode = nodes.get(parentHandle);

            if (parentNode) parentNode->addChild(toHandle);
            toNode->setParent(parentHandle);
        }

        // Resolve self-children relationships
        std::vector<TinyHandle> toChildren;
        for (const TinyHandle& childHandle : fromNode->childrenHandles) {
            if (childHandle.index >= toHandles.size()) continue;
            toNode->addChild(toHandles[childHandle.index]);
        }

        // Resolve components

        if (fromNode->has<TinyNode::Transform>()) {
            const auto* fromTransform = fromNode->get<TinyNode::Transform>();
            auto* toTransform = writeComp<TinyNode::Transform>(toHandle);
            *toTransform = *fromTransform;
        }

        if (fromNode->has<TinyNode::MeshRender>()) {
            const auto* fromMeshRender = fromNode->get<TinyNode::MeshRender>();
            auto* toMeshRender = writeComp<TinyNode::MeshRender>(toHandle);

            toMeshRender->pMeshHandle = fromMeshRender->pMeshHandle;

            if (validIndex(fromMeshRender->skeleNodeHandle, toHandles)) {
                toMeshRender->skeleNodeHandle = toHandles[fromMeshRender->skeleNodeHandle.index];
            }
        }

        // BoneAttach component
        if (fromNode->has<TinyNode::BoneAttach>()) {
            const auto* fromBoneAttach = fromNode->get<TinyNode::BoneAttach>();
            auto* toBoneAttach = writeComp<TinyNode::BoneAttach>(toHandle);

            if (validIndex(fromBoneAttach->skeleNodeHandle, toHandles)) {
                toBoneAttach->skeleNodeHandle = toHandles[fromBoneAttach->skeleNodeHandle.index];
            }

            toBoneAttach->boneIndex = fromBoneAttach->boneIndex;
        }

        // Skeleton component
        if (fromNode->has<TinyNode::Skeleton>()) {
            // Copy runtime skeleton data
            auto* toSkeleRT = writeComp<TinyNode::Skeleton>(toHandle);
            const auto* fromSkeleRT = from->nodeComp<TinyNode::Skeleton>(fromHandle);
            toSkeleRT->copy(fromSkeleRT);
        }
    }

    update(parentHandle); // Update transforms after adding new nodes
}





void TinyScene::updateRecursive(TinyHandle nodeHandle, const glm::mat4& parentGlobalTransform) {
    TinyHandle realHandle = nodeHandle.valid() ? nodeHandle : rootHandle();

    TinyNode* node = nodes.get(realHandle);
    if (!node) return;

    // Update transform component
    TinyNode::Transform* transform = nodeComp<TinyNode::Transform>(realHandle);
    glm::mat4 localMat = transform ? transform->local : glm::mat4(1.0f);
    glm::mat4 transformMat = parentGlobalTransform * localMat;

    // Update skeleton component
    TinySkeletonRT* rtSkele = nodeComp<TinyNode::Skeleton>(realHandle);
    if (rtSkele) rtSkele->update();

    // Update bone attachments transforms
    TinyNode::BoneAttach* boneAttach = nodeComp<TinyNode::BoneAttach>(realHandle);
    if (boneAttach) {
        TinyHandle skeleNodeHandle = boneAttach->skeleNodeHandle;
        TinySkeletonRT* skeleRT = nodeComp<TinyNode::Skeleton>(skeleNodeHandle);
        if (skeleRT) transformMat = transformMat * skeleRT->finalPose(boneAttach->boneIndex);
    }

    // Set global transform
    if (transform) transform->global = transformMat;

    // Recursively update all children
    for (const TinyHandle& childHandle : node->childrenHandles) {
        updateRecursive(childHandle, transformMat);
    }
}

void TinyScene::update(TinyHandle nodeHandle) {
    // Use root node if no valid handle provided
    TinyHandle realHandle = nodeHandle.valid() ? nodeHandle : rootHandle();
    
    TinyNode* node = nodes.get(realHandle);
    if (!node) return;

    // Update everything recursively
    TinyNode::Transform* parentTransform = nodeComp<TinyNode::Transform>(node->parentHandle);
    updateRecursive(realHandle, parentTransform ? parentTransform->global : glm::mat4(1.0f));
}


TinySkeletonRT* TinyScene::addSkeletonRT(TinyHandle nodeHandle) {
    TinyNode::Skeleton* compPtr = nodeCompRaw<TinyNode::Skeleton>(nodeHandle);
    if (!compPtr) return nullptr; // Unable to add skeleton component (should not happen)

    // Create new empty valid runtime skeleton
    TinySkeletonRT rtSkele;
    rtSkele.init(sceneReq.deviceVK, sceneReq.fsRegistry, sceneReq.skinDescPool, sceneReq.skinDescLayout);
    // Repurpose pHandle into runtime skeleton handle
    compPtr->pSkeleHandle = rtAdd<TinySkeletonRT>(std::move(rtSkele));

    // Return the runtime skeleton
    return rtGet<TinySkeletonRT>(compPtr->pSkeleHandle);
}

TinyAnimeRT* TinyScene::addAnimationRT(TinyHandle nodeHandle) {
    TinyNode::Animation* compPtr = nodeCompRaw<TinyNode::Animation>(nodeHandle);
    if (!compPtr) return nullptr; // Unable to add animation component (should not happen)

    TinyAnimeRT rtAnime;
    compPtr->pAnimeHandle = rtAdd<TinyAnimeRT>(std::move(rtAnime));

    return rtGet<TinyAnimeRT>(compPtr->pAnimeHandle);
}


// --------- Specific component's data access ---------

VkDescriptorSet TinyScene::nSkeleDescSet(TinyHandle nodeHandle) const {
    // Retrieve runtime skeleton data from TinyFS registry
    const TinySkeletonRT* rtSkele = nodeComp<TinyNode::Skeleton>(nodeHandle);
    return rtSkele ? rtSkele->descSet() : VK_NULL_HANDLE;
}

