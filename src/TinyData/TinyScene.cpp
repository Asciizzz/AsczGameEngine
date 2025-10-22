#include "TinyData/TinyScene.hpp"

#include <stdexcept>
#include <thread>

TinyHandle TinyScene::addRoot(const std::string& nodeName) {
    // Create a new root node
    TinyNode rootNode(nodeName);
    rootNode.add<TinyNode::Node3D>();
    setRoot(nodes.add(std::move(rootNode)));

    return rootHandle();
}

TinyHandle TinyScene::addNode(const std::string& nodeName, TinyHandle parentHandle) {
    TinyNode newNode(nodeName);
    newNode.add<TinyNode::Node3D>();

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

    nodeRemoveComp<TinyNode::Node3D>(nodeHandle);
    nodeRemoveComp<TinyNode::MeshRender>(nodeHandle);
    nodeRemoveComp<TinyNode::BoneAttach>(nodeHandle);
    nodeRemoveComp<TinyNode::Skeleton>(nodeHandle);

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
    return nodes.isValid(nodeHandle);
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
    if (!node) return false;

    node->setParent(newParentHandle);
    return true;
}

bool TinyScene::setNodeChildren(TinyHandle nodeHandle, const std::vector<TinyHandle>& newChildren) {
    TinyNode* node = nodes.get(nodeHandle);
    if (!node) return false;

    node->childrenHandles = newChildren;
    return true;
}




void TinyScene::addScene(TinyHandle sceneHandle, TinyHandle parentHandle) {
    const TinyScene* from = fs()->rGet<TinyScene>(sceneHandle);
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
        const TinyNode* fromNode = from->fromIndex(i);
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

        // Transform component
        if (fromNode->has<TinyNode::Node3D>()) {
            TinyNode::Node3D toTransform = fromNode->getCopy<TinyNode::Node3D>();
            nodeAddComp<TinyNode::Node3D>(toHandle, toTransform);
        }

        // MeshRender component
        if (fromNode->has<TinyNode::MeshRender>()) {
            TinyNode::MeshRender toMeshRender = fromNode->getCopy<TinyNode::MeshRender>();
            // Remap skeleton node handle
            if (toMeshRender.skeleNodeHandle.valid()) {
                toMeshRender.skeleNodeHandle = toHandles[toMeshRender.skeleNodeHandle.index];
            }

            nodeAddComp<TinyNode::MeshRender>(toHandle, toMeshRender);
        }

        // BoneAttach component
        if (fromNode->has<TinyNode::BoneAttach>()) {
            TinyNode::BoneAttach toBoneAttach = fromNode->getCopy<TinyNode::BoneAttach>();
            // Remap skeleton node handle
            if (toBoneAttach.skeleNodeHandle.valid()) {
                toBoneAttach.skeleNodeHandle = toHandles[toBoneAttach.skeleNodeHandle.index];
            }

            nodeAddComp<TinyNode::BoneAttach>(toHandle, toBoneAttach);
        }

        // Skeleton component
        if (fromNode->has<TinyNode::Skeleton>()) {
            TinyNode::Skeleton toSkeleton = fromNode->getCopy<TinyNode::Skeleton>();

            // This add function will create runtime skeleton data as needed
            nodeAddComp<TinyNode::Skeleton>(toHandle, toSkeleton);
        }
    }

    update(parentHandle); // Update transforms after adding new nodes
}





void TinyScene::updateRecursive(TinyHandle nodeHandle, const glm::mat4& parentGlobalTransform) {
    TinyHandle realHandle = nodeHandle.valid() ? nodeHandle : rootHandle();

    TinyNode* node = nodes.get(realHandle);
    if (!node) return;

    // Update transform component

    TinyNode::Node3D* transform = node->get<TinyNode::Node3D>();
    if (!transform) {
        throw std::runtime_error("Node missing Node3D component during updateRecursive");
    }

    transform->global = parentGlobalTransform * transform->local;

    // Retrieve skeleton component if exists
    TinyNode::Skeleton* skeletonComp = node->get<TinyNode::Skeleton>();
    if (skeletonComp) {
        TinySkeletonRT* rtSkele = getRT<TinySkeletonRT>(skeletonComp->rtSkeleHandle);
        if (rtSkele) rtSkele->update();
    }

    // Recursively update all children
    for (const TinyHandle& childHandle : node->childrenHandles) {
        updateRecursive(childHandle, transform->global);
    }
}

void TinyScene::update(TinyHandle nodeHandle) {
    // Use root node if no valid handle provided
    TinyHandle realHandle = nodeHandle.valid() ? nodeHandle : rootHandle();
    
    TinyNode* node = nodes.get(realHandle);
    if (!node) return;

// Update everything recursively

    glm::mat4 parentGlobal = glm::mat4(1.0f);
    const TinyNode* parent = nodes.get(node->parentHandle);
    if (parent) parentGlobal = parent->get<TinyNode::Node3D>()->global;

    updateRecursive(realHandle, parentGlobal);
}


TinyHandle TinyScene::nodeAddCompSkeleton(TinyHandle nodeHandle, TinyHandle skeletonHandle) {
    TinyNode* node = nodes.get(nodeHandle);
    if (!node) return TinyHandle();

    nodeRemoveCompSkeleton(nodeHandle); // Remove existing skeleton component if any

    TinyNode::Skeleton* compPtr = node->add<TinyNode::Skeleton>();
    
    const TinySkeleton* fsSkele = fs()->rGet<TinySkeleton>(skeletonHandle);
    if (!fsSkele) {
        compPtr->skeleHandle = TinyHandle();
        return TinyHandle();
    }

    // CRITICAL FIX: Set the skeleton handle BEFORE initializing runtime data
    compPtr->skeleHandle = skeletonHandle;

    TinySkeletonRT rtSkele;
    rtSkele.init(skeletonHandle, *fsSkele); // FIXED: Use skeletonHandle parameter, not compPtr->skeleHandle
    rtSkele.vkCreate(sceneReq.device, sceneReq.skinDescPool, sceneReq.skinDescLayout);

    compPtr->rtSkeleHandle = addRT<TinySkeletonRT>(std::move(rtSkele));

    TinySkeletonRT* rtSkelePtr = getRT<TinySkeletonRT>(compPtr->rtSkeleHandle);
    if (rtSkelePtr) rtSkelePtr->update(); // Initial update to set poses

    // Return the runtime skeleton handle
    return compPtr->rtSkeleHandle;
}

void TinyScene::nodeRemoveCompSkeleton(TinyHandle nodeHandle) {
    TinyNode* node = nodes.get(nodeHandle);
    if (!node) return;

    TinyNode::Skeleton* compPtr = node->get<TinyNode::Skeleton>();
    if (!compPtr) return; // No skeleton component exists

    // Remove skeleton runtime data using TinyFS pending deletion system
    if (compPtr->rtSkeleHandle.valid() && fs()->rHas<TinySkeletonRT>(compPtr->rtSkeleHandle)) {
        fs()->rRemove<TinySkeletonRT>(compPtr->rtSkeleHandle);
    }

    node->remove<TinyNode::Skeleton>();
}




VkDescriptorSet TinyScene::getNodeSkeletonDescSet(TinyHandle nodeHandle) const {
    // Retrieve skeleton component
    const TinyNode::Skeleton* compPtr = nodeComp<TinyNode::Skeleton>(nodeHandle);
    if (!compPtr) return VK_NULL_HANDLE;

    // Retrieve runtime skeleton data from TinyFS registry
    const TinySkeletonRT* rtSkele = getRT<TinySkeletonRT>(compPtr->rtSkeleHandle);
    if (!rtSkele) return VK_NULL_HANDLE;

    return rtSkele->descSet;
}