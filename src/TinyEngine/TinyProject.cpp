#include "TinyEngine/TinyProject.hpp"
#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <string>

using NTypes = TinyNode::Types;

// A quick function for range validation
template<typename T>
bool isValidIndex(int index, const std::vector<T>& vec) {
    return index >= 0 && index < static_cast<int>(vec.size());
}

// File dialog state
struct FileDialog {
    bool isOpen = false;
    std::filesystem::path currentPath;
    std::vector<std::filesystem::directory_entry> currentFiles;
    std::string selectedFile;
    TinyHandle targetFolder;
    
    void open(const std::filesystem::path& startPath, TinyHandle folder) {
        isOpen = true;
        currentPath = startPath;
        targetFolder = folder;
        selectedFile.clear();
        refreshFileList();
    }
    
    void refreshFileList() {
        currentFiles.clear();
        try {
            if (std::filesystem::exists(currentPath) && std::filesystem::is_directory(currentPath)) {
                for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
                    currentFiles.push_back(entry);
                }
                
                // Sort: directories first, then files, both alphabetically
                std::sort(currentFiles.begin(), currentFiles.end(), [](const auto& a, const auto& b) {
                    if (a.is_directory() != b.is_directory()) {
                        return a.is_directory(); // Directories first
                    }
                    return a.path().filename() < b.path().filename();
                });
            }
        } catch (const std::exception&) {
            // Handle permission errors or other filesystem issues
        }
    }
    
    bool isModelFile(const std::filesystem::path& path) {
        auto ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".glb" || ext == ".gltf";
    }
};

static FileDialog g_fileDialog;


TinyProject::TinyProject(const TinyVK::Device* deviceVK) : deviceVK(deviceVK) {
    // registry = MakeUnique<TinyRegistry>();
    tinyFS = MakeUnique<TinyFS>();

    TinyHandle registryHandle = tinyFS->addFolder(".registry");
    tinyFS->setRegistryHandle(registryHandle);

    // Create Main Scene (the active scene with a single root node)
    TinyRScene mainScene;
    mainScene.name = "Main Scene";
    mainScene.addRoot("Root");

    // Create "Main Scene" as a non-deletable file in root directory
    TinyFS::Node::CFG sceneConfig;
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

TinyHandle TinyProject::addSceneFromModel(const TinyModel& model, TinyHandle parentFolder) {
    // Use root folder if no valid parent provided
    if (!parentFolder.valid()) {
        parentFolder = tinyFS->rootHandle();
    }
    
    // Create a folder for the model in the specified parent
    TinyHandle fnModelFolder = tinyFS->addFolder(parentFolder, model.name);
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
        TinyHandle actualHandle = scene.nodes.add(rtNode);
        nodeHandles.push_back(actualHandle);
    }

    // Set the root node to the first node's actual handle
    if (!nodeHandles.empty()) {
        scene.rootHandle = nodeHandles[0];
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
    TinyHandle actualParent = parentNode.valid() ? parentNode : activeScene->rootHandle;
    
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
    if (!nodeHandle.valid()) nodeHandle = activeScene->rootHandle;

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
    if (nodeHandle != activeScene->rootHandle && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
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
    
    // Select node on right-click (immediate selection before context menu)
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        selectedSceneNodeHandle = nodeHandle;
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
            const TinyFS::Node* sceneFile = tinyFS->getFNodes().get(sceneFNodeHandle);
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
            const TinyFS::Node* fileNode = tinyFS->getFNodes().get(fileNodeHandle);
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
    
    // Context menu for nodes - direct manipulation
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
        
        if (ImGui::MenuItem("Add Child Node")) {
            TinyRScene* scene = getActiveScene();
            if (scene) {
                TinyHandle newNodeHandle = scene->addNode("New Node", nodeHandle);
                selectedSceneNodeHandle = newNodeHandle;
                expandNode(nodeHandle); // Expand parent to show new child
            }
        }
        
        // Only show delete for non-root nodes
        if (nodeHandle != activeScene->rootHandle) {
            if (ImGui::MenuItem("Delete Node")) {
                TinyRScene* scene = getActiveScene();
                if (scene && scene->removeNode(nodeHandle)) {
                    selectedSceneNodeHandle = TinyHandle(); // Clear selection
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
        // Create a sorted copy of children handles by node name
        std::vector<TinyHandle> sortedChildren(node->childrenHandles.begin(), node->childrenHandles.end());
        std::sort(sortedChildren.begin(), sortedChildren.end(), [activeScene](const TinyHandle& a, const TinyHandle& b) {
            const TinyRNode* nodeA = activeScene->nodes.get(a);
            const TinyRNode* nodeB = activeScene->nodes.get(b);
            if (!nodeA || !nodeB) return false;
            return nodeA->name < nodeB->name;
        });
        
        for (const TinyHandle& childHandle : sortedChildren) {
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



void TinyProject::processPendingDeletions() {
    if (pendingFNodeDeletions.empty()) return;
    
    TinyFS& fs = filesystem();
    
    for (TinyHandle handle : pendingFNodeDeletions) {
        fs.removeFNode(handle);
        
        // Clear selection if the selected file was deleted
        if (selectedFNodeHandle == handle) {
            selectedFNodeHandle = TinyHandle();
        }
    }

    pendingFNodeDeletions.clear();
}


void TinyProject::renderFileExplorerImGui(TinyHandle nodeHandle, int depth) {
    TinyFS& fs = filesystem();
    
    // Use root handle if no valid handle provided
    if (!nodeHandle.valid()) {
        nodeHandle = fs.rootHandle();
    }
    
    const TinyFS::Node* node = fs.getFNodes().get(nodeHandle);
    if (!node) return;
    
    // Create a unique ID for this node
    ImGui::PushID(static_cast<int>(nodeHandle.index));
    
    // Check if this node has children and is selected
    bool hasChildren = !node->children.empty();
    bool isSelected = (selectedFNodeHandle.index == nodeHandle.index && selectedFNodeHandle.version == nodeHandle.version);
    
    if (node->type == TinyFS::Node::Type::Folder) {
        // Display root folder as ".root" instead of full path
        std::string displayName = (nodeHandle == fs.rootHandle()) ? ".root" : node->name;
        
        // Create tree node flags
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        // Always show arrow for folders (even empty ones), but prevent tree push if no children
        if (!hasChildren) {
            flags |= ImGuiTreeNodeFlags_NoTreePushOnOpen; // Don't push tree state, but keep the arrow
        }
        if (isSelected) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        
        // Add consistent styling to match node hierarchy theme
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.4f)); // Gray hover background (same as nodes)
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.4f, 0.4f, 0.6f)); // Gray selection background (same as nodes)
        
        bool nodeOpen = ImGui::TreeNodeEx(displayName.c_str(), flags);
        
        // Pop the style colors for folders right after TreeNodeEx
        ImGui::PopStyleColor(2);
        
        // Handle selection - only select on mouse release if we didn't drag
        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            // Check if this was a click (no significant drag distance)
            ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
            float dragDistance = sqrtf(dragDelta.x * dragDelta.x + dragDelta.y * dragDelta.y);
            
            // Only select if drag distance was minimal (treat as click)
            if (dragDistance < 5.0f) { // 5 pixel tolerance
                selectedFNodeHandle = nodeHandle;
            }
            
            // Reset drag delta for next interaction
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        }
        
        // Select folder on right-click (immediate selection before context menu)
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            selectedFNodeHandle = nodeHandle;
        }
        
        // Drag source for folders
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("FOLDER_HANDLE", &nodeHandle, sizeof(TinyHandle));
            ImGui::Text("Moving: %s", displayName.c_str());
            ImGui::EndDragDropSource();
        }
        
        // Drag drop target for folders and files
        if (ImGui::BeginDragDropTarget()) {
            // Accept folders being dropped
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FOLDER_HANDLE")) {
                TinyHandle draggedFolder = *(const TinyHandle*)payload->Data;
                if (draggedFolder != nodeHandle) { // Can't drop folder on itself
                    fs.moveFNode(draggedFolder, nodeHandle);
                }
            }
            
            // Accept files being dropped  
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_HANDLE")) {
                TinyHandle draggedFile = *(const TinyHandle*)payload->Data;
                fs.moveFNode(draggedFile, nodeHandle);
            }
            
            ImGui::EndDragDropTarget();
        }
        
        // Context menu for folders - direct manipulation
        if (ImGui::BeginPopupContextItem()) {
            ImGui::Text("%s", displayName.c_str());
            ImGui::Separator();
            
            if (ImGui::MenuItem("Add Folder")) {
                TinyHandle newFolderHandle = fs.addFolder(nodeHandle, "New Folder");
                selectedFNodeHandle = newFolderHandle;
            }

            if (ImGui::MenuItem("Add Scene")) {
                TinyRScene newScene;
                newScene.name = "New Scene";
                newScene.addRoot("Root");
                
                // Add scene to registry and create file node
                TinyRScene* scenePtr = &newScene;
                TinyHandle fileHandle = fs.addFile(nodeHandle, "New Scene", scenePtr);
                selectedFNodeHandle = fileHandle;
            }
            
            if (ImGui::MenuItem("Load Model...")) {
                // TODO: Open file dialog to select model file
                // For now, show a placeholder message
                ImGui::OpenPopup("LoadModelDialog");
            }
            
            // Load Model Dialog (placeholder for now)
            if (ImGui::BeginPopupModal("LoadModelDialog", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Model loading functionality not yet implemented.");
                ImGui::Text("This would open a file dialog to select .glb/.gltf files");
                ImGui::Text("and load them into the folder: %s", displayName.c_str());
                
                ImGui::Separator();
                if (ImGui::Button("Close")) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            
            ImGui::Separator();
            if (ImGui::MenuItem("Delete", nullptr, false, node->deletable())) {
                if (node->deletable()) queueFNodeForDeletion(nodeHandle);
            }

            // Folder operations don't affect file, no need for pending deletion
            if (ImGui::MenuItem("Flatten", nullptr, false, node->deletable())) {
                if (node->deletable()) fs.flattenFNode(nodeHandle);
            }
            ImGui::EndPopup();
        }
        
        // If folder is open and has children, recurse for children
        if (nodeOpen && hasChildren) {
            // Create a sorted copy of children handles - folders first, then files, both sorted by name
            std::vector<TinyHandle> sortedChildren;
            for (const TinyHandle& childHandle : node->children) {
                const TinyFS::Node* child = fs.getFNodes().get(childHandle);
                if (!child || child->hidden()) continue; // Skip invalid or hidden nodes
                sortedChildren.push_back(childHandle);
            }
            
            std::sort(sortedChildren.begin(), sortedChildren.end(), [&fs](const TinyHandle& a, const TinyHandle& b) {
                const TinyFS::Node* nodeA = fs.getFNodes().get(a);
                const TinyFS::Node* nodeB = fs.getFNodes().get(b);
                if (!nodeA || !nodeB) return false;
                
                // Folders come before files
                if (nodeA->type != nodeB->type) {
                    return nodeA->type == TinyFS::Node::Type::Folder;
                }
                
                // Within same type, sort by name
                return nodeA->name < nodeB->name;
            });
            
            for (const TinyHandle& childHandle : sortedChildren) {
                renderFileExplorerImGui(childHandle, depth + 1);
            }
            ImGui::TreePop();
        }
        
    } else if (node->type == TinyFS::Node::Type::File) {
        if (node->tHandle.isType<TinyRScene>()) {
            // Scene file
            std::string sceneName = node->name;
            
            // Check if this is the active scene for green backdrop
            TinyHandle sceneRegistryHandle = node->tHandle.handle;
            bool isActiveScene = (getActiveSceneHandle() == sceneRegistryHandle);
            
            // Add permanent green backdrop for active scene files
            if (isActiveScene) {
                ImVec2 itemSize = ImGui::CalcTextSize(sceneName.c_str());
                ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                ImVec2 itemMax = ImVec2(cursorPos.x + ImGui::GetContentRegionAvail().x, cursorPos.y + itemSize.y);
                ImGui::GetWindowDrawList()->AddRectFilled(cursorPos, itemMax, IM_COL32(50, 200, 50, 100)); // green backdrop
            }
            
            // Set colors for scene files
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f)); // Gray text
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.4f)); // Hover background (same as nodes)
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.4f, 0.4f, 0.6f)); // Selection background (same as nodes)
            
            if (ImGui::Selectable(sceneName.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                selectedFNodeHandle = nodeHandle;
            }
            
            // Select scene file on right-click (immediate selection before context menu)
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                selectedFNodeHandle = nodeHandle;
            }
            
            // Drag source for scene files
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("FILE_HANDLE", &nodeHandle, sizeof(TinyHandle));
                ImGui::Text("Scene: %s", node->name.c_str());
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Drop on folders to move");
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Drop on nodes to instantiate");
                ImGui::EndDragDropSource();
            }
            
            // Context menu for scene files - direct manipulation
            if (ImGui::BeginPopupContextItem()) {
                ImGui::Text("%s", node->name.c_str());
                ImGui::Separator();
                
                // Get scene handle from TypeHandle
                TinyHandle sceneRegistryHandle = node->tHandle.handle;
                bool isCurrentlyActive = (getActiveSceneHandle() == sceneRegistryHandle);
                
                if (isCurrentlyActive) {
                    ImGui::TextColored(ImVec4(0.7f, 1.0f, 0.7f, 1.0f), "Active Scene");
                } else {
                    if (ImGui::MenuItem("Make Active Scene")) {
                        if (setActiveScene(sceneRegistryHandle)) {
                            selectedSceneNodeHandle = getRootNodeHandle(); // Reset node selection to root
                        }
                    }
                }
                
                ImGui::Separator();
                if (ImGui::MenuItem("Delete", nullptr, false, node->deletable())) {
                    if (node->deletable()) queueFNodeForDeletion(nodeHandle);
                }
                
                ImGui::EndPopup();
            }
            
            ImGui::PopStyleColor(3);
            
        } else {
            // Other file types
            std::string fileName = node->name;
            
            // Set colors for other files
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f)); // Gray text
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.4f)); // Hover background (same as nodes)
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.4f, 0.4f, 0.6f)); // Selection background (same as nodes)
            
            ImGui::Selectable(fileName.c_str(), isSelected, ImGuiSelectableFlags_None);
            
            // Handle file selection
            if (ImGui::IsItemClicked()) {
                selectedFNodeHandle = nodeHandle;
            }
            
            // Select file on right-click (immediate selection before context menu)
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                selectedFNodeHandle = nodeHandle;
            }
            
            // Drag source for other files
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("FILE_HANDLE", &nodeHandle, sizeof(TinyHandle));
                ImGui::Text("Moving file: %s", node->name.c_str());
                ImGui::EndDragDropSource();
            }
            
            // Context menu for other file types - direct manipulation
            if (ImGui::BeginPopupContextItem()) {
                ImGui::Text("%s", node->name.c_str());
                ImGui::Separator();
                
                if (ImGui::MenuItem("Delete", nullptr, false, node->deletable())) {
                    if (node->deletable()) pendingFNodeDeletions.push_back(nodeHandle);
                }
                ImGui::EndPopup();
            }
            
            ImGui::PopStyleColor(3);
        }
    }
    
    ImGui::PopID();
}

