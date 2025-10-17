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

    // Create CoreScene (the active scene with a single root node)
    TinyRScene coreScene;
    coreScene.name = "CoreScene";
    
    // Create root node for the core scene
    TinyRNode rootNode;
    rootNode.name = "Root";
    rootNode.localTransform = glm::mat4(1.0f);
    rootNode.globalTransform = glm::mat4(1.0f);
    
    coreScene.rootNode = coreScene.nodes.insert(std::move(rootNode));
    
    // Create "Main Scene" as a non-deletable file in root directory
    TinyFNode::CFG sceneConfig;
    sceneConfig.deletable = false; // Make it non-deletable
    
    TinyHandle mainSceneFileHandle = tinyFS->addFile(tinyFS->rootHandle(), "Main Scene", &coreScene, sceneConfig);
    TypeHandle mainSceneTypeHandle = tinyFS->getTHandle(mainSceneFileHandle);
    activeSceneHandle = mainSceneTypeHandle.handle; // Point to the actual scene in registry

    // Create camera and global UBO manager
    tinyCamera = MakeUnique<TinyCamera>(glm::vec3(0.0f, 0.0f, 0.0f), 45.0f, 0.1f, 100.0f);
    tinyGlobal = MakeUnique<TinyGlobal>(2);
    tinyGlobal->createVkResources(deviceVK);

    // Create default material and texture
    TinyTexture defaultTexture = TinyTexture::createDefaultTexture();
    TinyRTexture defaultRTexture = TinyRTexture(defaultTexture);
    defaultRTexture.create(deviceVK);

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

    // Note: fnHandle - handle to file node in TinyFS's fnodes
    //       tHandle - handle to the actual data in the registry (infused with Type info for TinyFS usage)

    // Import textures to registry
    std::vector<TinyHandle> glbTexRHandle;
    for (const auto& texture : model.textures) {
        TinyRTexture rTexture = TinyRTexture();
        rTexture.create(deviceVK);

        // TinyHandle handle = registry->add(textureData).handle;
        TinyHandle fnHandle = tinyFS->addFile(fnTexFolder, texture.name, std::move(&rTexture));
        TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

        glbTexRHandle.push_back(tHandle.handle);
    }

    // Import materials to registry with remapped texture references
    std::vector<TinyHandle> glmMatRHandle;
    for (const auto& material : model.materials) {
        TinyRMaterial correctMat;
        correctMat.name = material.name; // Copy material name

        // Remap the material's texture indices
        uint32_t localAlbIndex = material.localAlbTexture;
        bool localAlbValid = localAlbIndex >= 0 && localAlbIndex < static_cast<int>(glbTexRHandle.size());
        correctMat.setAlbTexIndex(localAlbValid ? glbTexRHandle[localAlbIndex].index : 0);

        uint32_t localNrmlIndex = material.localNrmlTexture;
        bool localNrmlValid = localNrmlIndex >= 0 && localNrmlIndex < static_cast<int>(glbTexRHandle.size());
        correctMat.setNrmlTexIndex(localNrmlValid ? glbTexRHandle[localNrmlIndex].index : 0);

        // TinyHandle handle = registry->add(correctMat).handle;
        TinyHandle fnHandle = tinyFS->addFile(fnMatFolder, material.name, &correctMat);
        TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

        glmMatRHandle.push_back(tHandle.handle);
    }

    // Import meshes to registry with remapped material references
    std::vector<TinyHandle> glbMeshRHandle;
    for (const auto& mesh : model.meshes) {
        TinyRMesh rMesh = TinyRMesh(mesh);
        rMesh.create(deviceVK);

        // Remap submeshes' material indices
        std::vector<TinySubmesh> remappedSubmeshes = mesh.submeshes;
        for (auto& submesh : remappedSubmeshes) {
            bool valid = isValidIndex(submesh.material.index, glmMatRHandle);
            submesh.material = valid ? glmMatRHandle[submesh.material.index] : TinyHandle();
        }

        rMesh.setSubmeshes(remappedSubmeshes);

        TinyHandle fnHandle = tinyFS->addFile(fnMeshFolder, mesh.name, std::move(&rMesh));
        TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

        glbMeshRHandle.push_back(tHandle.handle);
    }

    // Import skeletons to registry
    std::vector<TinyHandle> glbSkeleRHandle;
    for (const auto& skeleton : model.skeletons) {
        TinyRSkeleton rSkeleton = TinyRSkeleton(skeleton);

        // TinyHandle handle = registry->add(rSkeleton).handle;
        TinyHandle fnHandle = tinyFS->addFile(fnSkeleFolder, "Skeleton", &rSkeleton);
        TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

        glbSkeleRHandle.push_back(tHandle.handle);
    }

    // Create scene with nodes - preserve hierarchy but remap resource references
    TinyRScene scene;
    scene.name = model.name;

    // First pass: Insert all nodes and collect their actual handles
    std::vector<TinyHandle> nodeHandles;
    nodeHandles.reserve(model.nodes.size());

    for (const auto& node : model.nodes) {
        TinyRNode rtNode = node; // Copy node data

        // Remap MeshRender component's mesh reference
        if (rtNode.hasType(NTypes::MeshRender)) {
            auto* meshRender = rtNode.get<TinyRNode::MeshRender>();
            if (meshRender) meshRender->meshHandle = glbMeshRHandle[meshRender->meshHandle.index];
        }

        // Remap Skeleton component's registry reference
        if (rtNode.hasType(NTypes::Skeleton)) {
            auto* skeleton = rtNode.get<TinyRNode::Skeleton>();
            if (skeleton) {
                skeleton->skeleHandle = glbSkeleRHandle[skeleton->skeleHandle.index];

                // Construct the final bone transforms array (set to identity, resolve through skeleton later)
                const auto& skeleData = registryRef().get<TinyRSkeleton>(skeleton->skeleHandle);
                if (skeleData) skeleton->boneTransformsFinal.resize(skeleData->bones.size(), glm::mat4(1.0f));
            }
        }

        // Insert and capture the actual handle returned by the pool
        TinyHandle actualHandle = scene.nodes.insert(rtNode);
        nodeHandles.push_back(actualHandle);
    }

    // Set the root node to the first node's actual handle
    if (!nodeHandles.empty()) {
        scene.rootNode = nodeHandles[0];
    }

    // Second pass: Remap parent/child relationships using actual handles
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        TinyRNode* rtNode = scene.nodes.get(nodeHandles[i]);
        if (!rtNode) continue;

        const TinyNode& originalNode = model.nodes[i];

        // Clear existing children since we'll rebuild them with correct handles
        rtNode->childrenHandles.clear();

        // Remap parent handle
        if (originalNode.parentHandle.valid() && 
            originalNode.parentHandle.index < nodeHandles.size()) {
            rtNode->parentHandle = nodeHandles[originalNode.parentHandle.index];
        } else {
            rtNode->parentHandle = TinyHandle(); // Invalid handle for root
        }

        // Remap children handles
        for (const TinyHandle& childHandle : originalNode.childrenHandles) {
            if (childHandle.valid() && childHandle.index < nodeHandles.size()) {
                rtNode->childrenHandles.push_back(nodeHandles[childHandle.index]);
            }
        }
    }

    // Add scene to registry and return the handle
    TinyHandle fnHandle = tinyFS->addFile(fnModelFolder, scene.name, &scene);
    TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

    return tHandle.handle;
}



void TinyProject::addSceneInstance(TinyHandle sceneHandle, TinyHandle parentNode) {
    const auto& registry = registryRef();
    const TinyRScene* sourceScene = registry.get<TinyRScene>(sceneHandle);
    TinyRScene* activeScene = getActiveScene();
    
    if (!sourceScene || !activeScene) return;

    // Use root node if no valid parent provided
    TinyHandle actualParent = parentNode.valid() ? parentNode : activeScene->rootNode;
    
    // Use the scene's addSceneToNode method to merge the source scene into active scene
    activeScene->addSceneToNode(*sourceScene, actualParent);
    
    // Update transforms for the entire active scene
    activeScene->updateGlbTransform();
}

bool TinyProject::setActiveScene(TinyHandle sceneHandle) {
    // Verify the handle points to a valid TinyRScene in the registry
    const TinyRScene* scene = registryRef().get<TinyRScene>(sceneHandle);
    if (!scene) {
        return false; // Invalid handle or not a scene
    }
    
    // Switch the active scene
    activeSceneHandle = sceneHandle;

    // Update transforms for the new active scene
    TinyRScene* activeScene = getActiveScene();
    if (activeScene) activeScene->updateGlbTransform();
    
    return true;
}




void TinyProject::runPlayground(float dTime) {
    return;
}

void TinyProject::debugPrintHierarchyTree(TinyHandle nodeHandle, int depth) {
    TinyRScene* activeScene = getActiveScene();
    if (!activeScene) {
        std::cout << "No active scene!" << std::endl;
        return;
    }

    // Use root node if no valid handle provided
    if (!nodeHandle.valid()) {
        nodeHandle = activeScene->rootNode;
        std::cout << "=== SCENE HIERARCHY TREE ===" << std::endl;
        std::cout << "Active scene handle: " << activeSceneHandle.index << "_v" << activeSceneHandle.version << std::endl;
        std::cout << "Root node handle: " << nodeHandle.index << "_v" << nodeHandle.version << std::endl;
        std::cout << "Total nodes in pool: " << activeScene->nodes.count() << std::endl;
        std::cout << "--- Tree Structure ---" << std::endl;
    }

    const TinyRNode* node = activeScene->nodes.get(nodeHandle);
    if (!node) {
        for (int i = 0; i < depth; i++) std::cout << "  ";
        std::cout << "[INVALID NODE: " << nodeHandle.index << "_v" << nodeHandle.version << "]" << std::endl;
        return;
    }

    // Print indented node info
    for (int i = 0; i < depth; i++) std::cout << "  ";
    std::cout << "- " << node->name << " (" << nodeHandle.index << "_v" << nodeHandle.version << ")";
    
    // Add type info
    if (node->hasType(TinyNode::Types::MeshRender)) std::cout << " [Mesh]";
    if (node->hasType(TinyNode::Types::Skeleton)) std::cout << " [Skeleton]";
    if (node->hasType(TinyNode::Types::BoneAttach)) std::cout << " [BoneAttach]";
    
    // Parent info
    if (node->parentHandle.valid()) {
        std::cout << " (parent: " << node->parentHandle.index << "_v" << node->parentHandle.version << ")";
    } else {
        std::cout << " (ROOT)";
    }
    
    // Children count
    std::cout << " [" << node->childrenHandles.size() << " children]" << std::endl;

    // Recurse for children
    for (const TinyHandle& childHandle : node->childrenHandles) {
        debugPrintHierarchyTree(childHandle, depth + 1);
    }
}

void TinyProject::debugPrintHierarchyFlat() {
    TinyRScene* activeScene = getActiveScene();
    if (!activeScene) {
        std::cout << "No active scene!" << std::endl;
        return;
    }

    std::cout << "=== SCENE HIERARCHY FLAT ===" << std::endl;
    std::cout << "Active scene handle: " << activeSceneHandle.index << "_v" << activeSceneHandle.version << std::endl;
    std::cout << "Root node handle: " << activeScene->rootNode.index << "_v" << activeScene->rootNode.version << std::endl;
    std::cout << "Total pool capacity: " << activeScene->nodes.view().size() << std::endl;
    std::cout << "Active nodes in pool: " << activeScene->nodes.count() << std::endl;
    std::cout << "--- All Nodes (by pool index) ---" << std::endl;

    // Iterate through all pool slots
    for (uint32_t i = 0; i < activeScene->nodes.view().size(); ++i) {
        if (!activeScene->nodes.isOccupied(i)) continue;
        
        TinyHandle handle = activeScene->nodes.getHandle(i);
        const TinyRNode* node = activeScene->nodes.get(handle);
        
        if (!node) continue;

        std::cout << "[" << i << "] " << handle.index << "_v" << handle.version 
                  << " '" << node->name << "'";
        
        // Add type info
        if (node->hasType(TinyNode::Types::MeshRender)) std::cout << " [Mesh]";
        if (node->hasType(TinyNode::Types::Skeleton)) std::cout << " [Skeleton]";
        if (node->hasType(TinyNode::Types::BoneAttach)) std::cout << " [BoneAttach]";
        
        // Parent/children info
        std::cout << " | Parent: ";
        if (node->parentHandle.valid()) {
            std::cout << node->parentHandle.index << "_v" << node->parentHandle.version;
        } else {
            std::cout << "NONE";
        }
        
        std::cout << " | Children(" << node->childrenHandles.size() << "): ";
        for (size_t j = 0; j < node->childrenHandles.size(); ++j) {
            if (j > 0) std::cout << ", ";
            const TinyHandle& ch = node->childrenHandles[j];
            std::cout << ch.index << "_v" << ch.version;
        }
        std::cout << std::endl;
    }
    std::cout << "=========================" << std::endl;
}

void TinyProject::renderNodeTreeImGui(TinyHandle nodeHandle, int depth) {
    TinyRScene* activeScene = getActiveScene();
    if (!activeScene) return;
    
    // Use root node if no valid handle provided
    if (!nodeHandle.valid()) {
        nodeHandle = activeScene->rootNode;
    }
    
    const TinyRNode* node = activeScene->nodes.get(nodeHandle);
    if (!node) return;
    
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
    TinyRScene* activeScene = getActiveScene();
    if (!activeScene) return;
    
    // Use root node if no valid handle provided
    if (!nodeHandle.valid()) {
        nodeHandle = activeScene->rootNode;
    }
    
    const TinyRNode* node = activeScene->nodes.get(nodeHandle);
    if (!node) return;
    
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
    
    // Force open if this node is in the expanded set
    bool forceOpen = isNodeExpanded(nodeHandle);
    
    // Set the default open state (this will be overridden by user interaction)
    if (forceOpen) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
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
    
    // Track expansion state changes (only for nodes with children)
    if (hasChildren) {
        if (nodeOpen && !forceOpen) {
            // User expanded this node manually
            expandedNodes.insert(nodeHandle);
        } else if (!nodeOpen && isNodeExpanded(nodeHandle)) {
            // User collapsed this node manually
            expandedNodes.erase(nodeHandle);
        }
    }
    
    // Handle selection
    if (ImGui::IsItemClicked()) {
        selectedNode = nodeHandle;
    }
    
    // Drag and drop source (only if not root node)
    if (nodeHandle != activeScene->rootNode && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
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
            if (activeScene->reparentNode(draggedNode, nodeHandle)) {
                // Update global transforms after reparenting
                activeScene->updateGlbTransform();
                
                // Auto-expand the parent chain to show the newly dropped node
                expandParentChain(nodeHandle);
                
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
                const TinyRScene* scene = registryRef().get<TinyRScene>(sceneRegistryHandle);
                if (scene) {
                    // Place the scene at this node
                    addSceneInstance(sceneRegistryHandle, nodeHandle);
                    
                    // Auto-expand the parent chain to show the newly instantiated scene
                    expandParentChain(nodeHandle);
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
                
                // Safety check: prevent dropping a scene into itself
                if (sceneRegistryHandle == activeSceneHandle) {
                    // Cannot drop active scene into itself - ignore the operation
                    ImGui::SetTooltip("Cannot drop a scene into itself!");
                } else {
                    // Verify the scene exists and instantiate it at this node
                    const TinyRScene* scene = registryRef().get<TinyRScene>(sceneRegistryHandle);
                    if (scene) {
                        // Place the scene at this node
                        addSceneInstance(sceneRegistryHandle, nodeHandle);
                        
                        // Auto-expand the parent chain to show the newly instantiated scene
                        expandParentChain(nodeHandle);
                    }
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

void TinyProject::expandParentChain(TinyHandle nodeHandle) {
    TinyRScene* activeScene = getActiveScene();
    if (!activeScene) return;
    
    // Get the target node
    const TinyRNode* targetNode = activeScene->nodes.get(nodeHandle);
    if (!targetNode) return;
    
    // Walk up the parent chain and expand all parents
    TinyHandle currentHandle = targetNode->parentHandle;
    while (currentHandle.valid()) {
        expandedNodes.insert(currentHandle);
        
        const TinyRNode* currentNode = activeScene->nodes.get(currentHandle);
        if (!currentNode) break;
        
        currentHandle = currentNode->parentHandle;
    }
    
    // Also expand the target node itself if it has children
    if (!targetNode->childrenHandles.empty()) {
        expandedNodes.insert(nodeHandle);
    }
}

// Scene deletion can be handled through registry cleanup
// Runtime nodes are managed through the rtNodes vector directly