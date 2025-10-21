#include "TinyData/TinyScene.hpp"

TinyHandle TinyScene::addRoot(const std::string& nodeName) {
    // Create a new root node
    TinyNode rootNode(nodeName);
    setRoot(nodes.add(std::move(rootNode)));

    return rootHandle();
}

TinyHandle TinyScene::addNode(const std::string& nodeName, TinyHandle parentHandle) {
    TinyNode newNode;
    newNode.name = nodeName;

    return addNode(newNode, parentHandle);
}

TinyHandle TinyScene::addNode(const TinyNode& nodeData, TinyHandle parentHandle) {
    if (!parentHandle.valid()) parentHandle = rootHandle();

    TinyNode* parentNode = nodes.get(parentHandle);
    if (!parentNode) return TinyHandle(); // Invalid parent handle

    TinyNode newNode = nodeData;
    newNode.setParent(parentHandle);

// Resolve specific logic

    // If node has Skeleton component, create TinySkeletonRT in runtime registry
    if (newNode.has<TinyNode::Skeleton>()) {
        // Future implementation: Add TinySkeletonRT to runtime registry
    }


    // Add the node to the pool
    TinyHandle newNodeHandle = nodes.add(std::move(newNode));

    // Re-get parent pointer after pool reallocation because of memory and stuff
    parentNode = nodes.get(parentHandle);
    parentNode->addChild(newNodeHandle);

    return newNodeHandle;
}

TinyHandle TinyScene::addNodeRaw(const TinyNode& nodeData) {
    return nodes.add(nodeData);
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

    // Resolve specific removal logic
    if (nodeToDelete->has<TinyNode::Skeleton>()) {
        TinyNode::Skeleton* skelComp = nodeToDelete->get<TinyNode::Skeleton>();
        if (skelComp && skelComp->rtSkeleHandle.valid()) {
            // Future implementation: Remove TinySkeletonRT from runtime registry
            // rtRegistry.remove<TinySkeletonRT>(skelComp->rtSkeleHandle);
        }
    }

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



// TinyNode* TinyScene::node(TinyHandle nodeHandle) {
//     return nodes.get(nodeHandle);
// }

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




void TinyScene::updateGlbTransform(TinyHandle nodeHandle, const glm::mat4& parentGlobalTransform) {
    // Use root node if no valid handle provided
    TinyHandle actualNode = nodeHandle.valid() ? nodeHandle : rootHandle();
    
    TinyNode* node = nodes.get(actualNode);
    if (!node) return;
    
    // Calculate global transform: parent global * local transform
    node->globalTransform = parentGlobalTransform * node->localTransform;
    
    // Recursively update all children
    for (const TinyHandle& childHandle : node->childrenHandles) {
        updateGlbTransform(childHandle, node->globalTransform);
    }
}


void TinyScene::addScene(TinyHandle sceneHandle, TinyHandle parentHandle) {
    const TinyScene* sceneA = fsRegistry ? fsRegistry->get<TinyScene>(sceneHandle) : nullptr;

    if (!sceneA || sceneA->nodes.count() == 0) return;

    // Default to root node if no parent specified
    if (!parentHandle.valid()) parentHandle = rootHandle();

    // Create mapping from scene A handles to scene B handles
    std::unordered_map<uint32_t, TinyHandle> handleMap; // A_index -> B_handle
    
    // First pass: Insert all valid nodes from scene A into scene B and build the mapping
    const auto& sceneA_items = sceneA->nodeView();
    for (uint32_t i = 0; i < sceneA_items.size(); ++i) {
        if (!sceneA->nodes.isOccupied(i)) continue;

        TinyHandle oldHandle_A = sceneA->nodes.getHandle(i);
        if (!oldHandle_A.valid()) continue;

        const TinyNode* nodeA = sceneA->nodes.get(oldHandle_A);
        if (!nodeA) continue;
        
        // Copy the node and insert into scene B
        TinyNode nodeCopy = *nodeA;
        TinyHandle newHandle_B = nodes.add(std::move(nodeCopy));
        
        // Store the mapping: A's index -> B's handle
        handleMap[oldHandle_A.index] = newHandle_B;
    }
    
    // Second pass: Remap all node references using the handle mapping
    for (uint32_t i = 0; i < sceneA_items.size(); ++i) {
        TinyHandle oldHandle_A = sceneA->nodes.getHandle(i);
        const TinyNode* originalNodeA = sceneA->node(oldHandle_A);

        // Find our copied node in scene B
        auto it = handleMap.find(oldHandle_A.index);
        if (it == handleMap.end()) continue;
        
        TinyHandle newHandle_B = it->second;
        TinyNode* nodeB = nodes.get(newHandle_B);
        if (!nodeB) continue;
        
        // Clear existing children since we'll rebuild them
        nodeB->childrenHandles.clear();
        
        // Remap parent handle
        if (originalNodeA->parentHandle.valid()) {
            auto parentIt = handleMap.find(originalNodeA->parentHandle.index);
            if (parentIt != handleMap.end()) {
                // Parent is within the imported scene - remap to new handle
                nodeB->parentHandle = parentIt->second;
            } else {
                // Parent not in imported scene - this must be a root node, attach to specified parent
                nodeB->parentHandle = parentHandle;
            }
        } else {
            // No parent in original scene - attach to specified parent
            nodeB->parentHandle = parentHandle;
        }
        
        // Remap children handles
        for (const TinyHandle& childHandle_A : originalNodeA->childrenHandles) {
            auto childIt = handleMap.find(childHandle_A.index);
            if (childIt != handleMap.end()) {
                nodeB->childrenHandles.push_back(childIt->second);
            }
        }
        
        // Remap component node references
        if (nodeB->has<TinyNode::MeshRender>()) {
            auto* meshRender = nodeB->get<TinyNode::MeshRender>();
            if (meshRender && meshRender->skeleNodeHandle.valid()) {
                auto skeleIt = handleMap.find(meshRender->skeleNodeHandle.index);
                if (skeleIt != handleMap.end()) {
                    meshRender->skeleNodeHandle = skeleIt->second;
                }
            }
        }
        
        if (nodeB->has<TinyNode::BoneAttach>()) {
            auto* boneAttach = nodeB->get<TinyNode::BoneAttach>();
            if (boneAttach && boneAttach->skeleNodeHandle.valid()) {
                auto skeleIt = handleMap.find(boneAttach->skeleNodeHandle.index);
                if (skeleIt != handleMap.end()) {
                    boneAttach->skeleNodeHandle = skeleIt->second;
                }
            }
        }

        if (nodeB->has<TinyNode::Skeleton>()) {
            // Copy entire component data from scene A to B
            auto* skeletonComp = nodeB->get<TinyNode::Skeleton>();
            const TinyNode::Skeleton* originalSkeleComp = originalNodeA->get<TinyNode::Skeleton>();

            if (skeletonComp && originalSkeleComp) {
                skeletonComp->skeleHandle = originalSkeleComp->skeleHandle;
                // Runtime logic here
            }
        }
    }
    
    // Finally, add the scene A's root nodes as children of the specified parent
    TinyNode* parentNode = nodes.get(parentHandle);
    if (parentNode) {
        // Find nodes that were root nodes in scene A (had invalid parent or parent not in scene)
        for (const auto& [oldIndex, newHandle] : handleMap) {
            // Get the actual handle from scene A using the index
            TinyHandle oldHandle_A = sceneA->nodes.getHandle(oldIndex);
            if (!oldHandle_A.valid()) continue;

            const TinyNode* originalA = sceneA->nodes.get(oldHandle_A);

            if (originalA && (!originalA->parentHandle.valid() || 
                handleMap.find(originalA->parentHandle.index) == handleMap.end())) {
                // This was a root node in scene A, add it as child of our parent
                parentNode->childrenHandles.push_back(newHandle);
            }
        }
    }

    updateGlbTransform(parentHandle); // Update transforms after adding new nodes
}