#include "TinyEngine/TinyRData.hpp"

#include "TinyVK/System/CmdBuffer.hpp"

#include <stdexcept>

using namespace TinyVK;

bool TinyRMesh::create(const TinyVK::Device* deviceVK) {
    if (vertexData.empty() || indexData.empty()) return false;

    vertexBuffer
        .setDataSize(vertexData.size())
        .setUsageFlags(BufferUsage::Vertex)
        .createDeviceLocalBuffer(deviceVK, vertexData.data());

    indexBuffer
        .setDataSize(indexData.size())
        .setUsageFlags(BufferUsage::Index)
        .createDeviceLocalBuffer(deviceVK, indexData.data());

    return true;
}

VkIndexType TinyRMesh::tinyToVkIndexType(TinyMesh::IndexType type) {
    switch (type) {
        case TinyMesh::IndexType::Uint8:  return VK_INDEX_TYPE_UINT8;
        case TinyMesh::IndexType::Uint16: return VK_INDEX_TYPE_UINT16;
        case TinyMesh::IndexType::Uint32: return VK_INDEX_TYPE_UINT32;
        default: throw std::runtime_error("Unsupported index type in TinyMesh");
    }
}

bool TinyRTexture::create(const TinyVK::Device* deviceVK) {
    // Get appropriate Vulkan format and convert data if needed
    VkFormat textureFormat = ImageVK::getVulkanFormatFromChannels(channels);
    std::vector<uint8_t> vulkanData = ImageVK::convertToValidData(
        channels, width, height, data.data());

    // Calculate image size based on Vulkan format requirements
    int vulkanChannels = (channels == 3) ? 4 : channels; // RGB becomes RGBA
    VkDeviceSize imageSize = width * height * vulkanChannels;

    if (data.empty()) return false;

    // Create staging buffer for texture data upload
    DataBuffer stagingBuffer;
    stagingBuffer
        .setDataSize(imageSize * sizeof(uint8_t))
        .setUsageFlags(ImageUsage::TransferSrc)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(deviceVK)
        .uploadData(vulkanData.data());

    ImageConfig imageConfig = ImageConfig()
        .withPhysicalDevice(deviceVK->pDevice)
        .withDimensions(width, height)
        .withAutoMipLevels()
        .withFormat(textureFormat)
        .withUsage(ImageUsage::Sampled | ImageUsage::TransferDst | ImageUsage::TransferSrc)
        .withTiling(ImageTiling::Optimal)
        .withMemProps(MemProp::DeviceLocal);

    ImageViewConfig viewConfig = ImageViewConfig()
        .withAspectMask(ImageAspect::Color)
        .withAutoMipLevels(width, height);

    // A quick function to convert TinyTexture::AddressMode to VkSamplerAddressMode
    auto convertAddressMode = [](TinyTexture::AddressMode mode) {
        switch (mode) {
            case TinyTexture::AddressMode::Repeat:        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case TinyTexture::AddressMode::ClampToEdge:   return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case TinyTexture::AddressMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            default:                                      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    };

    SamplerConfig sampConfig = SamplerConfig()
    // The only thing we care about right now is address mode
        .withAddressModes(convertAddressMode(addressMode));

    textureVK = TextureVK(); // Reset texture
    bool success = textureVK
        .init(*deviceVK)
        .createImage(imageConfig)
        .createView(viewConfig)
        .createSampler(sampConfig)
        .valid();

    if (!success) return false;

    TempCmd tempCmd(deviceVK, deviceVK->graphicsPoolWrapper);

    textureVK
        .transitionLayoutImmediate(tempCmd.get(), ImageLayout::Undefined, ImageLayout::TransferDstOptimal)
        .copyFromBufferImmediate(tempCmd.get(), stagingBuffer.get())
        .generateMipmapsImmediate(tempCmd.get(), deviceVK->pDevice);

    tempCmd.endAndSubmit(); // Kinda redundant with RAII but whatever

    return true;
}

// TinyRScene implementation
void TinyRScene::addSceneToNode(const TinyRScene& sceneA, TinyHandle parentNodeHandle) {
    if (sceneA.nodes.count() == 0) return;

    // Use root node if no parent specified
    TinyHandle actualParent = parentNodeHandle.valid() ? parentNodeHandle : rootNode;
    
    // Create mapping from scene A handles to scene B handles
    std::unordered_map<uint32_t, TinyHandle> handleMap; // A_index -> B_handle
    
    // First pass: Insert all valid nodes from scene A into scene B and build the mapping
    const auto& sceneA_items = sceneA.nodes.view();
    for (uint32_t i = 0; i < sceneA_items.size(); ++i) {
        if (!sceneA.nodes.isOccupied(i)) continue;
        
        TinyHandle oldHandle_A = sceneA.nodes.getHandle(i);
        if (!oldHandle_A.valid()) continue;
        
        const TinyRNode* nodeA = sceneA.nodes.get(oldHandle_A);
        if (!nodeA) continue;
        
        // Copy the node and insert into scene B
        TinyRNode nodeCopy = *nodeA;
        TinyHandle newHandle_B = nodes.insert(std::move(nodeCopy));
        
        // Store the mapping: A's index -> B's handle
        handleMap[oldHandle_A.index] = newHandle_B;
    }
    
    // Second pass: Remap all node references using the handle mapping
    for (uint32_t i = 0; i < sceneA_items.size(); ++i) {
        if (!sceneA.nodes.isOccupied(i)) continue;
        
        TinyHandle oldHandle_A = sceneA.nodes.getHandle(i);
        if (!oldHandle_A.valid()) continue;
        
        const TinyRNode* originalNodeA = sceneA.nodes.get(oldHandle_A);
        if (!originalNodeA) continue;
        
        // Find our copied node in scene B
        auto it = handleMap.find(oldHandle_A.index);
        if (it == handleMap.end()) continue;
        
        TinyHandle newHandle_B = it->second;
        TinyRNode* nodeB = nodes.get(newHandle_B);
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
                nodeB->parentHandle = actualParent;
            }
        } else {
            // No parent in original scene - attach to specified parent
            nodeB->parentHandle = actualParent;
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
    }
    
    // Finally, add the scene A's root nodes as children of the specified parent
    TinyRNode* parentNode = nodes.get(actualParent);
    if (parentNode) {
        // Find nodes that were root nodes in scene A (had invalid parent or parent not in scene)
        for (const auto& [oldIndex, newHandle] : handleMap) {
            // Get the actual handle from scene A using the index
            TinyHandle oldHandle_A = sceneA.nodes.getHandle(oldIndex);
            if (!oldHandle_A.valid()) continue;
            
            const TinyRNode* originalA = sceneA.nodes.get(oldHandle_A);
            
            if (originalA && (!originalA->parentHandle.valid() || 
                handleMap.find(originalA->parentHandle.index) == handleMap.end())) {
                // This was a root node in scene A, add it as child of our parent
                parentNode->childrenHandles.push_back(newHandle);
            }
        }
    }
}

void TinyRScene::updateGlbTransform(TinyHandle nodeHandle, const glm::mat4& parentGlobalTransform) {
    // Use root node if no valid handle provided
    TinyHandle actualNode = nodeHandle.valid() ? nodeHandle : rootNode;
    
    TinyRNode* node = nodes.get(actualNode);
    if (!node) return;
    
    // Calculate global transform: parent global * local transform
    node->globalTransform = parentGlobalTransform * node->localTransform;
    
    // Recursively update all children
    for (const TinyHandle& childHandle : node->childrenHandles) {
        updateGlbTransform(childHandle, node->globalTransform);
    }
}

bool TinyRScene::deleteNodeRecursive(TinyHandle nodeHandle) {
    TinyRNode* nodeToDelete = nodes.get(nodeHandle);
    if (!nodeToDelete) {
        return false; // Invalid handle
    }

    // Don't allow deletion of root node
    if (nodeHandle == rootNode) {
        return false;
    }

    // First, recursively delete all children
    // We need to copy the children handles because we'll be modifying the vector during iteration
    std::vector<TinyHandle> childrenToDelete = nodeToDelete->childrenHandles;
    for (const TinyHandle& childHandle : childrenToDelete) {
        deleteNodeRecursive(childHandle); // This will remove each child from the pool
    }

    // Remove this node from its parent's children list
    if (nodeToDelete->parentHandle.valid()) {
        TinyRNode* parentNode = nodes.get(nodeToDelete->parentHandle);
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

bool TinyRScene::reparentNode(TinyHandle nodeHandle, TinyHandle newParentHandle) {
    // Validate handles
    if (!nodeHandle.valid() || !newParentHandle.valid()) {
        return false;
    }
    
    // Can't reparent root node
    if (nodeHandle == rootNode) {
        return false;
    }
    
    // Can't reparent to itself
    if (nodeHandle == newParentHandle) {
        return false;
    }
    
    TinyRNode* nodeToMove = nodes.get(nodeHandle);
    TinyRNode* newParent = nodes.get(newParentHandle);
    
    if (!nodeToMove || !newParent) {
        return false;
    }
    
    // Check for cycles: make sure new parent is not a descendant of the node we're moving
    std::function<bool(TinyHandle)> isDescendant = [this, newParentHandle, &isDescendant](TinyHandle ancestor) -> bool {
        const TinyRNode* node = nodes.get(ancestor);
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
        TinyRNode* currentParent = nodes.get(nodeToMove->parentHandle);
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
