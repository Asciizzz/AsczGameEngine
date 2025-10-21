#include "TinyData/TinyScene.hpp"


void TinyScene::updateGlbTransform(TinyHandle nodeHandle, const glm::mat4& parentGlobalTransform) {
    // Use root node if no valid handle provided
    TinyHandle actualNode = nodeHandle.valid() ? nodeHandle : rootHandle;
    
    TinyNode* node = nodes.get(actualNode);
    if (!node) return;
    
    // Calculate global transform: parent global * local transform
    node->globalTransform = parentGlobalTransform * node->localTransform;
    
    // Recursively update all children
    for (const TinyHandle& childHandle : node->childrenHandles) {
        updateGlbTransform(childHandle, node->globalTransform);
    }
}

TinyHandle TinyScene::addRoot(const std::string& nodeName) {
    // Create a new root node
    TinyNode rootNode;
    rootNode.name = nodeName;

    rootHandle = nodes.add(std::move(rootNode));

    return rootHandle;
}

TinyHandle TinyScene::addNode(const std::string& nodeName, TinyHandle parentHandle) {
    // If no parent specified, use root node
    if (!parentHandle.valid()) parentHandle = rootHandle;
    
    // Validate parent exists
    TinyNode* parentNode = nodes.get(parentHandle);
    if (!parentNode) {
        return TinyHandle(); // Invalid parent handle
    }
    
    // Create a new blank node
    TinyNode newNode;
    newNode.name = nodeName;
    newNode.localTransform = glm::mat4(1.0f); // Identity transform
    newNode.globalTransform = glm::mat4(1.0f);
    newNode.parentHandle = parentHandle;
    newNode.types = 0; // No components
    
    // Add the node to the pool first
    TinyHandle newNodeHandle = nodes.add(std::move(newNode));
    
    // Re-get parent pointer after potential pool reallocation
    parentNode = nodes.get(parentHandle);
    if (!parentNode) {
        // This shouldn't happen, but safety check
        nodes.remove(newNodeHandle);
        return TinyHandle();
    }
    
    // Add to parent's children list
    parentNode->childrenHandles.push_back(newNodeHandle);
    
    return newNodeHandle;
}

TinyHandle TinyScene::addNode(const TinyNode& nodeData, TinyHandle parentHandle) {
    // If no parent specified, use root node
    if (!parentHandle.valid()) parentHandle = rootHandle;
    
    // Validate parent exists
    TinyNode* parentNode = nodes.get(parentHandle);
    if (!parentNode) {
        return TinyHandle(); // Invalid parent handle
    }
    
    // Create a copy of the provided node data
    TinyNode newNode = nodeData;
    newNode.parentHandle = parentHandle; // Ensure correct parent
    // Note: childrenHandles will be empty initially
    
    // Add the node to the pool
    TinyHandle newNodeHandle = nodes.add(std::move(newNode));
    
    // Re-get parent pointer after potential pool reallocation
    parentNode = nodes.get(parentHandle);
    if (!parentNode) {
        // This shouldn't happen, but safety check
        nodes.remove(newNodeHandle);
        return TinyHandle();
    }
    
    // Add to parent's children list
    parentNode->childrenHandles.push_back(newNodeHandle);
    
    return newNodeHandle;
}


void TinyScene::addScene(TinyHandle sceneHandle, TinyHandle parentHandle) {
    const TinyScene* sceneA = fsRegistry ? fsRegistry->get<TinyScene>(sceneHandle) : nullptr;

    if (!sceneA || sceneA->nodes.count() == 0) return;

    // Default to root node if no parent specified
    if (!parentHandle.valid()) parentHandle = rootHandle;

    // Create mapping from scene A handles to scene B handles
    std::unordered_map<uint32_t, TinyHandle> handleMap; // A_index -> B_handle
    
    // First pass: Insert all valid nodes from scene A into scene B and build the mapping
    const auto& sceneA_items = sceneA->nodes.view();
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
        if (!sceneA->nodes.isOccupied(i)) continue;

        TinyHandle oldHandle_A = sceneA->nodes.getHandle(i);
        if (!oldHandle_A.valid()) continue;

        const TinyNode* originalNodeA = sceneA->getNode(oldHandle_A);
        if (!originalNodeA) continue;
        
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
        if (nodeB->hasComponent<TinyNode::MeshRender>()) {
            auto* meshRender = nodeB->get<TinyNode::MeshRender>();
            if (meshRender && meshRender->skeleNodeHandle.valid()) {
                auto skeleIt = handleMap.find(meshRender->skeleNodeHandle.index);
                if (skeleIt != handleMap.end()) {
                    meshRender->skeleNodeHandle = skeleIt->second;
                }
            }
        }
        
        if (nodeB->hasComponent<TinyNode::BoneAttach>()) {
            auto* boneAttach = nodeB->get<TinyNode::BoneAttach>();
            if (boneAttach && boneAttach->skeleNodeHandle.valid()) {
                auto skeleIt = handleMap.find(boneAttach->skeleNodeHandle.index);
                if (skeleIt != handleMap.end()) {
                    boneAttach->skeleNodeHandle = skeleIt->second;
                }
            }
        }

        if (nodeB->hasComponent<TinyNode::Skeleton>()) {
            // Copy entire component data from scene A to B
            auto* skeletonComp = nodeB->get<TinyNode::Skeleton>();
            const TinyNode::Skeleton* originalSkeleComp = originalNodeA->get<TinyNode::Skeleton>();

            if (skeletonComp && originalSkeleComp) {
                skeletonComp->skeleHandle = originalSkeleComp->skeleHandle;
                skeletonComp->localPose = originalSkeleComp->localPose;
                skeletonComp->finalPose = originalSkeleComp->finalPose;
                skeletonComp->skinData = originalSkeleComp->skinData;
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

bool TinyScene::removeNode(TinyHandle nodeHandle, bool recursive) {
    TinyNode* nodeToDelete = nodes.get(nodeHandle);
    if (!nodeToDelete || nodeHandle == rootHandle) return false;

    std::vector<TinyHandle> childrenToDelete = nodeToDelete->childrenHandles;
    for (const TinyHandle& childHandle : childrenToDelete) {
        // removeNode(childHandle); // This will remove each child from the pool

        // If recursive, remove children, otherwise attach them to the deleted node's parent
        if (recursive) {
            removeNode(childHandle, true);
        } else {
            reparentNode(childHandle, nodeToDelete->parentHandle);
        }
    }

    // Remove this node from its parent's children list
    if (nodeToDelete->parentHandle.valid()) {
        TinyNode* parentNode = nodes.get(nodeToDelete->parentHandle);
        if (parentNode) {
            auto& parentChildren = parentNode->childrenHandles;
            parentChildren.erase(
                std::remove(parentChildren.begin(), parentChildren.end(), nodeHandle),
                parentChildren.end()
            );
        }
    }

    // Finally, remove the node from the pool
    nodes.remove(nodeHandle);

    return true;
}

bool TinyScene::flattenNode(TinyHandle nodeHandle) {
    return removeNode(nodeHandle, false);
}

bool TinyScene::reparentNode(TinyHandle nodeHandle, TinyHandle newParentHandle) {
    // Validate handles
    if (!nodeHandle.valid() || !newParentHandle.valid()) {
        return false;
    }
    
    // Can't reparent root node
    if (nodeHandle == rootHandle) {
        return false;
    }
    
    // Can't reparent to itself
    if (nodeHandle == newParentHandle) {
        return false;
    }
    
    TinyNode* nodeToMove = nodes.get(nodeHandle);
    TinyNode* newParent = nodes.get(newParentHandle);
    
    if (!nodeToMove || !newParent) {
        return false;
    }
    
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
    if (nodeToMove->parentHandle.valid()) {
        TinyNode* currentParent = nodes.get(nodeToMove->parentHandle);
        if (currentParent) {
            auto& parentChildren = currentParent->childrenHandles;
            parentChildren.erase(
                std::remove(parentChildren.begin(), parentChildren.end(), nodeHandle),
                parentChildren.end()
            );
        }
    }
    
    // Add to new parent's children list
    newParent->childrenHandles.push_back(nodeHandle);

    // Update the node's parent handle
    nodeToMove->parentHandle = newParentHandle;
    
    return true;
}


TinyNode* TinyScene::getNode(TinyHandle nodeHandle) {
    return nodes.get(nodeHandle);
}

const TinyNode* TinyScene::getNode(TinyHandle nodeHandle) const {
    return nodes.get(nodeHandle);
}

bool TinyScene::renameNode(TinyHandle nodeHandle, const std::string& newName) {
    TinyNode* node = nodes.get(nodeHandle);
    if (!node) return false;

    node->name = newName;
    return true;
}