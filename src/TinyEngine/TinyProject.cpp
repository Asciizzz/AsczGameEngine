#include "TinyEngine/TinyProject.hpp"
#include <imgui.h>
#include <algorithm>

using NTypes = TinyNode::Types;

// A quick function for range validation
template<typename T>
bool isValidIndex(int index, const std::vector<T>& vec) {
    return index >= 0 && index < static_cast<int>(vec.size());
}


TinyProject::TinyProject(const TinyVK::Device* deviceVK) : deviceVK(deviceVK) {
    // registry = MakeUnique<TinyRegistry>();
    tinyFS = MakeUnique<TinyFS>();

    TinyHandle registryHandle = tinyFS->addFolder(".registry");
    tinyFS->setRegistryHandle(registryHandle);

    // Create root runtime node
    TinyNodeRT rootNode;
    rootNode.name = "Root";
    // Children will be added upon scene population

    rootNodeHandle = rtNodes.insert(std::move(rootNode)); // Store the root handle

    // Create camera and global UBO manager
    tinyCamera = MakeUnique<TinyCamera>(glm::vec3(0.0f, 0.0f, 0.0f), 45.0f, 0.1f, 100.0f);
    tinyGlobal = MakeUnique<TinyGlobal>(2);
    tinyGlobal->createVkResources(deviceVK);

    // Create default material and texture
    TinyTexture defaultTexture = TinyTexture::createDefaultTexture();
    TinyRTexture defaultRTexture;
    defaultRTexture.import(deviceVK, defaultTexture);

    defaultTextureHandle = tinyFS->addToRegistry(defaultRTexture).handle;

    TinyRMaterial defaultMaterial;
    defaultMaterial.setAlbTexIndex(0);
    defaultMaterial.setNrmlTexIndex(0);

    defaultMaterialHandle = tinyFS->addToRegistry(defaultMaterial).handle;
}

TinyHandle TinyProject::addSceneFromModel(const TinyModel& model) {
    // Create a folder for the model
    TinyHandle fnModelFolder = tinyFS->addFolder(model.name);
    TinyHandle fnTexFolder = tinyFS->addFolder(fnModelFolder, "Textures");
    TinyHandle fnMatFolder = tinyFS->addFolder(fnModelFolder, "Materials");
    TinyHandle fnMeshFolder = tinyFS->addFolder(fnModelFolder, "Meshes");
    TinyHandle fnSkeleFolder = tinyFS->addFolder(fnModelFolder, "Skeletons");
    TinyHandle fnSceneFolder = tinyFS->addFolder(fnModelFolder, "Scenes");

    // Note: fnHandle - handle to file node in TinyFS's fnodes
    //       tHandle - handle to the actual data in the registry (infused with Type info for TinyFS usage)

    // Import textures to registry
    std::vector<TinyHandle> glbTexrHandle;
    for (const auto& texture : model.textures) {
        TinyRTexture textureData;
        textureData.import(deviceVK, texture);

        // TinyHandle handle = registry->add(textureData).handle;
        TinyHandle fnHandle = tinyFS->addFile(fnTexFolder, texture.name, &textureData);
        TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

        glbTexrHandle.push_back(tHandle.handle);
    }

    // Import materials to registry with remapped texture references
    std::vector<TinyHandle> glbMatrHandle;
    for (const auto& material : model.materials) {
        TinyRMaterial correctMat;
        correctMat.name = material.name; // Copy material name

        // Remap the material's texture indices
        uint32_t localAlbIndex = material.localAlbTexture;
        bool localAlbValid = localAlbIndex >= 0 && localAlbIndex < static_cast<int>(glbTexrHandle.size());
        correctMat.setAlbTexIndex(localAlbValid ? glbTexrHandle[localAlbIndex].index : 0);

        uint32_t localNrmlIndex = material.localNrmlTexture;
        bool localNrmlValid = localNrmlIndex >= 0 && localNrmlIndex < static_cast<int>(glbTexrHandle.size());
        correctMat.setNrmlTexIndex(localNrmlValid ? glbTexrHandle[localNrmlIndex].index : 0);

        // TinyHandle handle = registry->add(correctMat).handle;
        TinyHandle fnHandle = tinyFS->addFile(fnMatFolder, material.name, &correctMat);
        TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

        glbMatrHandle.push_back(tHandle.handle);
    }

    // Import meshes to registry with remapped material references
    std::vector<TinyHandle> glbMeshrHandle;
    for (const auto& mesh : model.meshes) {
        TinyRMesh meshData;
        meshData.import(deviceVK, mesh);

        // Remap submeshes' material indices
        std::vector<TinySubmesh> remappedSubmeshes = mesh.submeshes;
        for (auto& submesh : remappedSubmeshes) {
            bool valid = isValidIndex(submesh.material.index, glbMatrHandle);
            submesh.material = valid ? glbMatrHandle[submesh.material.index] : TinyHandle();
        }

        meshData.setSubmeshes(remappedSubmeshes);

        TinyHandle fnHandle = tinyFS->addFile(fnMeshFolder, mesh.name, std::move(&meshData));
        TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

        glbMeshrHandle.push_back(tHandle.handle);
    }

    // Import skeletons to registry
    std::vector<TinyHandle> glbSkelerHandle;
    for (const auto& skeleton : model.skeletons) {
        TinyRSkeleton rSkeleton;
        rSkeleton.bones = skeleton.bones;

        // TinyHandle handle = registry->add(rSkeleton).handle;
        TinyHandle fnHandle = tinyFS->addFile(fnSkeleFolder, "Skeleton", &rSkeleton);
        TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

        glbSkelerHandle.push_back(tHandle.handle);
    }

    // Create scene with nodes - preserve hierarchy but remap resource references
    TinyRScene scene;
    scene.name = model.name;
    scene.nodes = model.nodes; // Start with a copy of the original hierarchy

    // Only remap resource references in components, keep hierarchy intact
    for (auto& node : scene.nodes) {
        // Remap MeshRender component's mesh reference
        if (node.hasType(NTypes::MeshRender)) {
            auto* meshRender = node.get<TinyNode::MeshRender>();
            if (meshRender && isValidIndex(meshRender->mesh.index, glbMeshrHandle)) {
                meshRender->mesh = glbMeshrHandle[meshRender->mesh.index];
            }
            // Note: skeleNode references remain as local indices within the scene
        }

        // Remap Skeleton component's registry reference
        if (node.hasType(NTypes::Skeleton)) {
            auto* skeleton = node.get<TinyNode::Skeleton>();
            if (skeleton && isValidIndex(skeleton->skeleRegistry.index, glbSkelerHandle)) {
                skeleton->skeleRegistry = glbSkelerHandle[skeleton->skeleRegistry.index];
            }
        }
    }

    // Add scene to registry and return the handle
    TinyHandle fnHandle = tinyFS->addFile(fnSceneFolder, scene.name, &scene);
    TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

    return tHandle.handle;
}



void TinyProject::addSceneInstance(TinyHandle sceneHandle, TinyHandle rootHandle, glm::mat4 at) {
    // const TinyRScene* scene = registry->get<TinyRScene>(sceneHandle);

    const auto& registry = tinyFS->registryRef();
    const TinyRScene* scene = registry.get<TinyRScene>(sceneHandle);

    if (!scene || !scene->hasNodes()) {
        printf("Error: Invalid scene handle %llu or empty scene\n", sceneHandle.value);
        return;
    }

    // Use the stored root handle if no valid handle is provided
    TinyHandle actualRootHandle = rootHandle.valid() ? rootHandle : rootNodeHandle;

    // Create mapping from scene node index to runtime node handle
    UnorderedMap<int32_t, TinyHandle> sceneToRuntimeMap;

    // First pass: Create all runtime nodes and copy scene node data
    for (int32_t i = 0; i < static_cast<int32_t>(scene->nodes.size()); ++i) {
        const TinyNode& sceneNode = scene->nodes[i];
        
        TinyNodeRT rtNode;
        rtNode.copyFromSceneNode(sceneNode);
        
        TinyHandle rtHandle = rtNodes.insert(std::move(rtNode));
        sceneToRuntimeMap[i] = rtHandle;
    }

    // Second pass: Remap all references (parent/children + component skeleton references)
    for (int32_t i = 0; i < static_cast<int32_t>(scene->nodes.size()); ++i) {
        const TinyNode& sceneNode = scene->nodes[i];
        TinyHandle rtHandle = sceneToRuntimeMap[i];
        TinyNodeRT* rtNode = rtNodes.get(rtHandle);
        
        if (!rtNode) continue; // Safety check

        // Remap parent-child relationships
        if (sceneNode.parent.valid() && sceneNode.parent.index < scene->nodes.size()) {
            // Has parent within scene - remap to runtime parent
            TinyHandle rtParentHandle = sceneToRuntimeMap[sceneNode.parent.index];
            rtNode->parentHandle = rtParentHandle;
            
            TinyNodeRT* parentNode = rtNodes.get(rtParentHandle);
            if (parentNode) {
                parentNode->addChild(rtHandle, rtNodes);
            }
        } else {
            // No parent in scene - attach to project root
            rtNode->parentHandle = actualRootHandle;
            
            TinyNodeRT* rootNode = rtNodes.get(actualRootHandle);
            if (rootNode) {
                rootNode->addChild(rtHandle, rtNodes);
            }
        }

        // Apply instance transform to scene root (first node)
        if (i == scene->getRootNodeIndex()) {
            rtNode->localTransform = at * rtNode->localTransform;
        }

        // Remap skeleton node references in components
        if (rtNode->hasType(NTypes::MeshRender)) {
            auto* meshRender = rtNode->get<TinyNodeRT::MeshRender>();
            if (meshRender) {
                const auto* sceneMeshRender = sceneNode.get<TinyNode::MeshRender>();
                if (sceneMeshRender && sceneMeshRender->skeleNode.valid() && 
                    sceneMeshRender->skeleNode.index < scene->nodes.size()) {
                    meshRender->skeleNodeRT = sceneToRuntimeMap[sceneMeshRender->skeleNode.index];
                }
                rtMeshRenderHandles.push_back(rtHandle);
            }
        }

        if (rtNode->hasType(NTypes::BoneAttach)) {
            auto* boneAttach = rtNode->get<TinyNodeRT::BoneAttach>();
            if (boneAttach) {
                const auto* sceneBoneAttach = sceneNode.get<TinyNode::BoneAttach>();
                if (sceneBoneAttach && sceneBoneAttach->skeleNode.valid() && 
                    sceneBoneAttach->skeleNode.index < scene->nodes.size()) {
                    boneAttach->skeleNodeRT = sceneToRuntimeMap[sceneBoneAttach->skeleNode.index];
                }
            }
        }

        if (rtNode->hasType(NTypes::Skeleton)) {
            auto* skeleton = rtNode->get<TinyNodeRT::Skeleton>();
            if (skeleton) {
                const auto& skeleData = registry.get<TinyRSkeleton>(skeleton->skeleRegistry);

                if (skeleData)
                    skeleton->boneTransformsFinal.resize(skeleData->bones.size(), glm::mat4(1.0f));
            }
        }
    }

    // Update transforms for the entire hierarchy  
    updateGlobalTransforms(actualRootHandle);
}


void TinyProject::updateGlobalTransforms(TinyHandle rootNodeHandle, const glm::mat4& parentGlobalTransform) {
    TinyNodeRT* runtimeNode = rtNodes.get(rootNodeHandle);
    if (!runtimeNode) return;

    // Use the local transform which contains the original scene node transform + any instance overrides
    glm::mat4 localTransform = runtimeNode->localTransform;

    // Calculate global transform: parent global * local transform
    runtimeNode->globalTransform = parentGlobalTransform * localTransform;

    // Recursively update all children
    for (const TinyHandle& childHandle : runtimeNode->childrenHandles) {
        updateGlobalTransforms(childHandle, runtimeNode->globalTransform);
    }
}

bool TinyProject::deleteNodeRecursive(TinyHandle nodeHandle) {
    TinyNodeRT* nodeToDelete = rtNodes.get(nodeHandle);
    if (!nodeToDelete) {
        return false; // Invalid handle
    }

    // Don't allow deletion of root node
    if (nodeHandle == rootNodeHandle) {
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
        TinyNodeRT* parentNode = rtNodes.get(nodeToDelete->parentHandle);
        if (parentNode) {
            auto& parentChildren = parentNode->childrenHandles;
            parentChildren.erase(
                std::remove(parentChildren.begin(), parentChildren.end(), nodeHandle),
                parentChildren.end()
            );
        }
    }

    // Remove from mesh render handles if this node has a MeshRender component
    if (nodeToDelete->hasComponent<TinyNodeRT::MeshRender>()) {
        rtMeshRenderHandles.erase(
            std::remove(rtMeshRenderHandles.begin(), rtMeshRenderHandles.end(), nodeHandle),
            rtMeshRenderHandles.end()
        );
    }

    // Finally, remove the node from the pool
    rtNodes.remove(nodeHandle);

    return true;
}

bool TinyProject::reparentNode(TinyHandle nodeHandle, TinyHandle newParentHandle) {
    // Validate handles
    if (!nodeHandle.valid() || !newParentHandle.valid()) {
        return false;
    }
    
    // Can't reparent root node
    if (nodeHandle == rootNodeHandle) {
        return false;
    }
    
    // Can't reparent to itself
    if (nodeHandle == newParentHandle) {
        return false;
    }
    
    TinyNodeRT* nodeToMove = rtNodes.get(nodeHandle);
    TinyNodeRT* newParent = rtNodes.get(newParentHandle);
    
    if (!nodeToMove || !newParent) {
        return false;
    }
    
    // Check for cycles: make sure new parent is not a descendant of the node we're moving
    std::function<bool(TinyHandle)> isDescendant = [this, newParentHandle, &isDescendant](TinyHandle ancestor) -> bool {
        const TinyNodeRT* node = rtNodes.get(ancestor);
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
        TinyNodeRT* currentParent = rtNodes.get(nodeToMove->parentHandle);
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

// TinyNodeRT method implementation
void TinyNodeRT::addChild(TinyHandle childHandle, TinyPool<TinyNodeRT>& rtNodesPool) {
    // Add child to this node's children list
    childrenHandles.push_back(childHandle);
}

void TinyProject::runPlayground(float dTime) {
    return;
}





void TinyProject::renderNodeTreeImGui(TinyHandle nodeHandle, int depth) {
    // Use root node if no valid handle provided
    if (!nodeHandle.valid()) {
        nodeHandle = rootNodeHandle;
    }
    
    const TinyNodeRT* node = rtNodes.get(nodeHandle);
    if (!node) {
        return;
    }
    
    // Create a unique ID for this node
    ImGui::PushID(static_cast<int>(nodeHandle.index));
    
    // Check if this node has children
    bool hasChildren = !node->childrenHandles.empty();
    
    // Create tree node or leaf
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    
    // Extract position from global transform for display
    glm::vec3 worldPos = glm::vec3(node->globalTransform[3]);
    
    // Create the node label with useful information
    std::string label = node->name;
    if (node->hasType(TinyNode::Types::MeshRender)) {
        label += " [Mesh]";
    }
    if (node->hasType(TinyNode::Types::Skeleton)) {
        label += " [Skeleton]";
    }
    if (node->hasType(TinyNode::Types::BoneAttach)) {
        label += " [BoneAttach]";
    }
    
    bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), flags);
    
    // Show node details in tooltip
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Handle: %u_v%u", nodeHandle.index, nodeHandle.version);
        ImGui::Text("Parent: %u_v%u", node->parentHandle.index, node->parentHandle.version);
        ImGui::Text("Children: %zu", node->childrenHandles.size());
        
        ImGui::Text("World Position: (%.2f, %.2f, %.2f)", worldPos.x, worldPos.y, worldPos.z);
        ImGui::Text("Type Mask: 0x%X", node->types);
        ImGui::EndTooltip();
    }
    
    // If node is open and has children, recurse for children
    if (nodeOpen && hasChildren) {
        for (const TinyHandle& childHandle : node->childrenHandles) {
            renderNodeTreeImGui(childHandle, depth + 1);
        }
        ImGui::TreePop();
    }
    
    ImGui::PopID();
}

void TinyProject::renderSelectableNodeTreeImGui(TinyHandle nodeHandle, TinyHandle& selectedNode, int depth) {
    // Use root node if no valid handle provided
    if (!nodeHandle.valid()) {
        nodeHandle = rootNodeHandle;
    }
    
    const TinyNodeRT* node = rtNodes.get(nodeHandle);
    if (!node) {
        return;
    }
    
    // Create a unique ID for this node
    ImGui::PushID(static_cast<int>(nodeHandle.index));
    
    // Check if this node has children
    bool hasChildren = !node->childrenHandles.empty();
    bool isSelected = (selectedNode.index == nodeHandle.index && selectedNode.version == nodeHandle.version);
    
    // Create tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (isSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    
    // Extract position from global transform for display
    glm::vec3 worldPos = glm::vec3(node->globalTransform[3]);
    
    // Create the node label with useful information
    std::string label = node->name;
    if (node->hasType(TinyNode::Types::MeshRender)) {
        label += " [Mesh]";
    }
    if (node->hasType(TinyNode::Types::Skeleton)) {
        label += " [Skeleton]";
    }
    if (node->hasType(TinyNode::Types::BoneAttach)) {
        label += " [BoneAttach]";
    }
    
    bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), flags);
    
    // Handle selection
    if (ImGui::IsItemClicked()) {
        selectedNode = nodeHandle;
    }
    
    // Drag and drop source (only if not root node)
    if (nodeHandle != rootNodeHandle && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        // Set payload to carry the node handle
        ImGui::SetDragDropPayload("NODE_HANDLE", &nodeHandle, sizeof(TinyHandle));
        
        // Display preview
        ImGui::Text("Moving: %s", node->name.c_str());
        ImGui::EndDragDropSource();
    }
    
    // Drag and drop target
    if (ImGui::BeginDragDropTarget()) {
        // Accept node reparenting
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("NODE_HANDLE")) {
            TinyHandle draggedNode = *(const TinyHandle*)payload->Data;
            
            // Attempt to reparent the dragged node to this node
            if (reparentNode(draggedNode, nodeHandle)) {
                // Update global transforms after reparenting
                updateGlobalTransforms(rootNodeHandle);
                
                // If the dragged node was selected, keep it selected
                if (selectedNode == draggedNode) {
                    selectedNode = draggedNode;
                }
            }
        }
        
        // Accept scene drops (from filesystem nodes)
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_FNODE")) {
            TinyHandle sceneFNodeHandle = *(const TinyHandle*)payload->Data;
            
            // Get the filesystem node to access its TypeHandle
            const TinyFNode* sceneFile = tinyFS->getFNodes().get(sceneFNodeHandle);
            if (sceneFile && sceneFile->isFile() && sceneFile->tHandle.isType<TinyRScene>()) {
                // Extract the registry handle from the TypeHandle
                TinyHandle sceneRegistryHandle = sceneFile->tHandle.handle;
                
                // Verify the scene exists and instantiate it at this node
                const TinyRScene* scene = tinyFS->registryRef().get<TinyRScene>(sceneRegistryHandle);
                if (scene) {
                    // Place the scene at this node
                    addSceneInstance(sceneRegistryHandle, nodeHandle, glm::mat4(1.0f));
                    updateGlobalTransforms(rootNodeHandle);
                }
            }
        }
        
        // Also accept FILE_HANDLE payloads and check if they're scene files
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_HANDLE")) {
            TinyHandle fileNodeHandle = *(const TinyHandle*)payload->Data;
            
            // Get the filesystem node to check if it's a scene file
            const TinyFNode* fileNode = tinyFS->getFNodes().get(fileNodeHandle);
            if (fileNode && fileNode->isFile() && fileNode->tHandle.isType<TinyRScene>()) {
                // This is a scene file - instantiate it at this node
                TinyHandle sceneRegistryHandle = fileNode->tHandle.handle;
                
                // Verify the scene exists and instantiate it at this node
                const TinyRScene* scene = tinyFS->registryRef().get<TinyRScene>(sceneRegistryHandle);
                if (scene) {
                    // Place the scene at this node
                    addSceneInstance(sceneRegistryHandle, nodeHandle, glm::mat4(1.0f));
                    updateGlobalTransforms(rootNodeHandle);
                }
            }
        }
        
        ImGui::EndDragDropTarget();
    }
    
    // Show node details in tooltip
    if (ImGui::IsItemHovered() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImGui::BeginTooltip();
        ImGui::Text("Handle: %u_v%u", nodeHandle.index, nodeHandle.version);
        ImGui::Text("Parent: %u_v%u", node->parentHandle.index, node->parentHandle.version);
        ImGui::Text("Children: %zu", node->childrenHandles.size());
        ImGui::Text("World Position: (%.2f, %.2f, %.2f)", worldPos.x, worldPos.y, worldPos.z);
        ImGui::Text("Type Mask: 0x%X", node->types);
        ImGui::EndTooltip();
    }
    
    // If node is open and has children, recurse for children
    if (nodeOpen && hasChildren) {
        for (const TinyHandle& childHandle : node->childrenHandles) {
            renderSelectableNodeTreeImGui(childHandle, selectedNode, depth + 1);
        }
        ImGui::TreePop();
    }
    
    ImGui::PopID();
}

// Scene deletion can be handled through registry cleanup
// Runtime nodes are managed through the rtNodes vector directly