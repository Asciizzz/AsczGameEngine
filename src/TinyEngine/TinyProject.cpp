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

    // Create Main Scene (the active scene with a single root node)
    TinyRScene mainScene;
    mainScene.name = "Main Scene";
    
    // Create root node for the main scene 
    TinyRNode rootNode;
    rootNode.name = "Root";
    mainScene.rootNode = mainScene.nodes.insert(std::move(rootNode));

    // Create "Main Scene" as a non-deletable file in root directory
    TinyFNode::CFG sceneConfig;
    sceneConfig.deletable = false; // Make it non-deletable
    
    TinyHandle mainSceneFileHandle = tinyFS->addFile(tinyFS->rootHandle(), "Main Scene", &mainScene, sceneConfig);
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
    
    // Use the scene's addScene method to merge the source scene into active scene
    activeScene->addScene(*sourceScene, actualParent);
    
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
    bool isSelected = (selectedSceneNodeHandle.index == nodeHandle.index && selectedSceneNodeHandle.version == nodeHandle.version);
    bool isHeld = (heldSceneNodeHandle.index == nodeHandle.index && heldSceneNodeHandle.version == nodeHandle.version);
    
    // Create tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (isSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    
    // Add consistent styling to match File explorer theme
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.4f)); // Gray hover background (same as File explorer)
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.4f, 0.4f, 0.6f)); // Gray selection background (same as File explorer)
    
    // Force open if this node is in the expanded set
    bool forceOpen = isNodeExpanded(nodeHandle);
    
    // Set the default open state (this will be overridden by user interaction)
    if (forceOpen) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }

    bool nodeOpen = ImGui::TreeNodeEx(node->name.c_str(), flags);

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
    
    // Drag and drop source (only if not root node)
    bool isDragging = false;
    if (nodeHandle != activeScene->rootNode && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        isDragging = true;
        heldSceneNodeHandle = nodeHandle;  // Set the held node during drag
        
        // Set payload to carry the node handle
        ImGui::SetDragDropPayload("NODE_HANDLE", &nodeHandle, sizeof(TinyHandle));
        
        // Display preview
        ImGui::Text("Moving: %s", node->name.c_str());
        ImGui::EndDragDropSource();
    }
    
    // Clear held node when not actively dragging from this item
    // Note: held node will be cleared in the drag drop target accept logic
    
    // Handle selection - only select on mouse release if we didn't drag
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        // Check if this was a click (no significant drag distance)
        ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
        float dragDistance = sqrtf(dragDelta.x * dragDelta.x + dragDelta.y * dragDelta.y);
        
        // Only select if drag distance was minimal (treat as click)
        if (dragDistance < 5.0f) { // 5 pixel tolerance
            selectedSceneNodeHandle = nodeHandle;
        }
        
        // Reset drag delta for next interaction
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
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
                
                // Keep the dragged node selected (maintain selection across drag operations)
                selectedSceneNodeHandle = draggedNode;
                
                // Clear held state after successful drop
                heldSceneNodeHandle = TinyHandle();
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
    
    // Select node on right-click (before context menu appears)
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        selectedSceneNodeHandle = nodeHandle;
    }
    
    // Context menu for individual nodes
    if (ImGui::BeginPopupContextItem()) {
        ImGui::Text("%s", node->name.c_str());
        ImGui::Separator();
        
        // Show component types in context menu
        std::string typeLabel = "";
        if (node->hasType(TinyNode::Types::MeshRender)) {
            typeLabel += "[Mesh] ";
        }
        if (node->hasType(TinyNode::Types::BoneAttach)) {
            typeLabel += "[BoneAttach] ";
        }
        if (node->hasType(TinyNode::Types::Skeleton)) {
            typeLabel += "[Skeleton] ";
        }
        
        if (!typeLabel.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", typeLabel.c_str());
            ImGui::Separator();
        }
        
        if (ImGui::MenuItem("Add Child")) {
            if (onAddChildNode) {
                onAddChildNode(nodeHandle);
            }
        }
        
        // Only show delete for non-root nodes
        if (nodeHandle != activeScene->rootNode) {
            if (ImGui::MenuItem("Delete Node")) {
                if (onDeleteNode) {
                    onDeleteNode(nodeHandle);
                }
            }
        }
        
        ImGui::EndPopup();
    }
    
    // Show node details in tooltip (slicker version like the old function)
    if (ImGui::IsItemHovered() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImGui::BeginTooltip();

        // Create the node label with useful information
        std::string typeLabel = "";
        if (node->hasType(TinyNode::Types::MeshRender)) {
            typeLabel += "[Mesh] ";
        }
        if (node->hasType(TinyNode::Types::BoneAttach)) {
            typeLabel += "[BoneAttach] ";
        }
        if (node->hasType(TinyNode::Types::Skeleton)) {
            typeLabel += "[Skeleton] ";
        }

        typeLabel = typeLabel.empty() ? "[None]" : typeLabel;
        
        ImGui::Text("Types: %s", typeLabel.c_str());

        if (!node->childrenHandles.empty()) {
            ImGui::Text("Children: %zu", node->childrenHandles.size());
        }
        
        ImGui::EndTooltip();
    }
    
    // If node is open and has children, recurse for children
    if (nodeOpen && hasChildren) {
        for (const TinyHandle& childHandle : node->childrenHandles) {
            renderNodeTreeImGui(childHandle, depth + 1);
        }
        ImGui::TreePop();
    }
    
    // Pop the style colors we pushed earlier
    ImGui::PopStyleColor(2); // Pop HeaderHovered and Header
    
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

void TinyProject::renderFileExplorerImGui(int depth) {
    TinyFS& fs = filesystem();
    
    // Start with root folder
    renderFileFolderTree(fs, fs.rootHandle(), depth);
    
    // Context menu for empty space in file explorer
    if (ImGui::BeginPopupContextWindow("FileExplorerContextMenu")) {
        ImGui::Text("File Explorer");
        ImGui::Separator();
        
        if (ImGui::MenuItem("Add Folder")) {
            if (onAddFolder) {
                // Add to selected folder if one is selected, otherwise add to root
                TinyHandle targetParent = selectedFNodeHandle.valid() ? selectedFNodeHandle : TinyHandle();
                onAddFolder(targetParent);
            }
        }
        if (ImGui::MenuItem("Add Scene")) {
            if (onAddScene) {
                // Add to selected folder if one is selected, otherwise add to root
                TinyHandle targetParent = selectedFNodeHandle.valid() ? selectedFNodeHandle : TinyHandle();
                onAddScene(targetParent);
            }
        }
        
        ImGui::EndPopup();
    }
}

void TinyProject::renderFileFolderTree(TinyFS& fs, TinyHandle folderHandle, int depth) {
    const TinyFNode* folder = fs.getFNodes().get(folderHandle);
    if (!folder) return;
    
    // Skip root node display
    if (folderHandle != fs.rootHandle()) {
        ImGui::PushID(folderHandle.index);
        
        // Check if this folder is selected
        bool isFolderSelected = (selectedFNodeHandle == folderHandle);
        
        // Use consistent styling with node hierarchy
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // White text
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.4f)); // Hover background
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.4f, 0.4f, 0.6f)); // Gray selection background
        
        bool nodeOpen = ImGui::TreeNodeEx(folder->name.c_str(), 
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
            (isFolderSelected ? ImGuiTreeNodeFlags_Selected : 0));
        
        ImGui::PopStyleColor(3);
        
        // Handle folder selection
        if (ImGui::IsItemClicked()) {
            selectedFNodeHandle = folderHandle;
        }
        
        // Select folder on right-click (before context menu appears)
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            selectedFNodeHandle = folderHandle;
        }
        
        // Drag source for folders
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("FOLDER_HANDLE", &folderHandle, sizeof(TinyHandle));
            ImGui::Text("Moving folder: %s", folder->name.c_str());
            ImGui::EndDragDropSource();
        }
        
        // Drag drop target for folders and files
        if (ImGui::BeginDragDropTarget()) {
            // Accept folders being dropped
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FOLDER_HANDLE")) {
                TinyHandle draggedFolder = *(const TinyHandle*)payload->Data;
                if (draggedFolder != folderHandle) { // Can't drop folder on itself
                    fs.moveFNode(draggedFolder, folderHandle);
                }
            }
            
            // Accept files being dropped  
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_HANDLE")) {
                TinyHandle draggedFile = *(const TinyHandle*)payload->Data;
                fs.moveFNode(draggedFile, folderHandle);
            }
            
            ImGui::EndDragDropTarget();
        }
        
        // Context menu for folders - same style as node hierarchy
        if (ImGui::BeginPopupContextItem()) {
            ImGui::Text("%s", folder->name.c_str());
            ImGui::Separator();
            
            if (ImGui::MenuItem("Add Folder")) {
                if (onAddFolder) {
                    onAddFolder(folderHandle);
                }
            }
            if (ImGui::MenuItem("Add Scene")) {
                if (onAddScene) {
                    onAddScene(folderHandle);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
                if (folder->isDeletable() && onDeleteFile) {
                    onDeleteFile(folderHandle);
                }
            }
            ImGui::EndPopup();
        }
        
        if (nodeOpen) {
            // Render children with proper indentation
            for (TinyHandle childHandle : folder->children) {
                const TinyFNode* child = fs.getFNodes().get(childHandle);
                if (!child || child->isHidden()) continue; // Skip invalid or hidden nodes

                if (child->type == TinyFNode::Type::Folder) {
                    renderFileFolderTree(fs, childHandle, depth + 1);
                } else if (child->type == TinyFNode::Type::File) {
                    renderFileItem(fs, childHandle);
                }
            }
            ImGui::TreePop();
        }
        
        ImGui::PopID();
    } else {
        // Root folder - just render children without tree node
        for (TinyHandle childHandle : folder->children) {
            const TinyFNode* child = fs.getFNodes().get(childHandle);
            if (!child || child->isHidden()) continue; // Skip invalid or hidden nodes

            if (child->type == TinyFNode::Type::Folder) {
                renderFileFolderTree(fs, childHandle, depth);
            } else if (child->type == TinyFNode::Type::File) {
                renderFileItem(fs, childHandle);
            }
        }
    }
}

void TinyProject::renderFileItem(TinyFS& fs, TinyHandle fileHandle) {
    const TinyFNode* file = fs.getFNodes().get(fileHandle);
    if (!file) return;
    
    ImGui::PushID(fileHandle.index);
    
    bool isSelected = (selectedFNodeHandle == fileHandle);
    
    if (file->tHandle.isType<TinyRScene>()) {
        // Scene file
        TinyRScene* scene = static_cast<TinyRScene*>(fs.registryRef().get(file->tHandle));
        if (scene) {
            std::string sceneName = file->name;
            
            // Check if this is the active scene for red backdrop
            TinyHandle sceneRegistryHandle = file->tHandle.handle;
            bool isActiveScene = (getActiveSceneHandle() == sceneRegistryHandle);
            
            // Add permanent red backdrop for active scene files
            if (isActiveScene) {
                ImVec2 itemSize = ImGui::CalcTextSize(sceneName.c_str());
                ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                ImVec2 itemMax = ImVec2(cursorPos.x + ImGui::GetContentRegionAvail().x, cursorPos.y + itemSize.y);
                ImGui::GetWindowDrawList()->AddRectFilled(cursorPos, itemMax, IM_COL32(200, 50, 50, 100)); // Red backdrop
            }
            
            // Set colors for scene files: gray text, normal selection behavior
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f)); // Gray text
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.4f)); // Subtle hover background
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.4f, 0.4f, 0.6f)); // Gray selection background
            
            if (ImGui::Selectable(sceneName.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                selectedFNodeHandle = fileHandle;
                
                // Double-click handling would need to be passed to Application
                if (ImGui::IsMouseDoubleClicked(0)) {
                    // This would need a callback for double-click scene placement
                }
            }
            
            // Select scene file on right-click (before context menu appears)
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                selectedFNodeHandle = fileHandle;
            }
            
            ImGui::PopStyleColor(3);
            
            // Drag source for scene files
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("FILE_HANDLE", &fileHandle, sizeof(TinyHandle));
                ImGui::Text("Scene: %s", file->name.c_str());
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Drop on folders to move");
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Drop on nodes to instantiate");
                ImGui::EndDragDropSource();
            }
            
            // Context menu for scene files - same style as node hierarchy
            if (ImGui::BeginPopupContextItem()) {
                ImGui::Text("%s", file->name.c_str());
                ImGui::Separator();
                
                // Get scene handle from TypeHandle
                TinyHandle sceneRegistryHandle = file->tHandle.handle;
                bool isCurrentlyActive = (getActiveSceneHandle() == sceneRegistryHandle);
                
                if (isCurrentlyActive) {
                    ImGui::TextColored(ImVec4(0.7f, 1.0f, 0.7f, 1.0f), "Active Scene");
                } else {
                    if (ImGui::MenuItem("Make Active Scene")) {
                        setActiveScene(sceneRegistryHandle);
                    }
                }
                
                ImGui::Separator();
                
                if (ImGui::MenuItem("Duplicate Scene")) {
                    // TODO: Implement scene duplication
                }
                
                if (ImGui::MenuItem("Delete")) {
                    if (file->isDeletable() && onDeleteFile) {
                        onDeleteFile(fileHandle);
                    }
                }
                
                ImGui::EndPopup();
            }
        }
    } else {
        // Other file types
        std::string fileName = file->name;
        
        // Set colors for other files: gray text, gray selection background
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f)); // Gray text
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.3f)); // Subtle hover background
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.4f, 0.4f, 0.6f)); // Gray selection background
        
        ImGui::Selectable(fileName.c_str(), isSelected, ImGuiSelectableFlags_None);
        
        // Handle file selection
        if (ImGui::IsItemClicked()) {
            selectedFNodeHandle = fileHandle;
        }
        
        // Select file on right-click (before context menu appears)
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            selectedFNodeHandle = fileHandle;
        }
        
        ImGui::PopStyleColor(3);
        
        // Drag source for other files
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("FILE_HANDLE", &fileHandle, sizeof(TinyHandle));
            ImGui::Text("Moving file: %s", file->name.c_str());
            ImGui::EndDragDropSource();
        }
        
        // Context menu for other file types - same style as node hierarchy
        if (ImGui::BeginPopupContextItem()) {
            ImGui::Text("%s", file->name.c_str());
            ImGui::Separator();
            
            if (ImGui::MenuItem("Delete")) {
                if (file->isDeletable() && onDeleteFile) {
                    onDeleteFile(fileHandle);
                }
            }
            ImGui::EndPopup();
        }
    }
    
    ImGui::PopID();
}