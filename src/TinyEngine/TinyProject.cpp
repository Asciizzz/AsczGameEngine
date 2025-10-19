#include "TinyEngine/TinyProject.hpp"
#include "TinyEngine/TinyLoader.hpp"
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
    bool justOpened = false;  // Flag to track when we need to call ImGui::OpenPopup
    bool shouldClose = false; // Flag to handle delayed closing
    std::filesystem::path currentPath;
    std::vector<std::filesystem::directory_entry> currentFiles;
    std::string selectedFile;
    TinyHandle targetFolder;
    
    void open(const std::filesystem::path& startPath, TinyHandle folder) {
        // Don't open if we're in the process of closing
        if (shouldClose) return;
        
        isOpen = true;
        justOpened = true;  // Mark that we need to open the popup
        currentPath = startPath;
        targetFolder = folder;
        selectedFile.clear();
        refreshFileList();
    }
    
    void close() {
        shouldClose = true;  // Mark for closing instead of immediate close
        selectedFile.clear();
        targetFolder = TinyHandle();
    }
    
    void update() {
        // Handle delayed closing after ImGui has processed the popup
        if (shouldClose && !ImGui::IsPopupOpen("Load Model File")) {
            isOpen = false;
            justOpened = false;
            shouldClose = false;
        }
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
    TinyScene mainScene;
    mainScene.name = "Main Scene";
    mainScene.addRoot("Root");

    // Create "Main Scene" as a non-deletable file in root directory
    TinyFS::Node::CFG sceneConfig;
    sceneConfig.deletable = false; // Make it non-deletable

    TinyHandle mainSceneFileHandle = tinyFS->addFile(tinyFS->rootHandle(), "Main Scene", &mainScene, sceneConfig);
    TypeHandle mainSceneTypeHandle = tinyFS->getTHandle(mainSceneFileHandle);
    activeSceneHandle = mainSceneTypeHandle.handle; // Point to the actual scene in registry

    // Initialize unified selection system - select the root scene node by default
    selectedHandle = SelectHandle(mainScene.rootHandle, SelectHandle::Type::Scene);

    // Create camera and global UBO manager
    tinyCamera = MakeUnique<TinyCamera>(glm::vec3(0.0f, 0.0f, 0.0f), 45.0f, 0.1f, 100.0f);
    tinyGlobal = MakeUnique<TinyGlobal>(2);
    tinyGlobal->createVkResources(deviceVK);

    // Create default material and texture
    TinyTexture defaultTexture = TinyTexture::createDefaultTexture();
    defaultTexture.vkCreate(deviceVK);

    defaultTextureHandle = tinyFS->addToRegistry(defaultTexture).handle;

    TinyRMaterial defaultMaterial;
    defaultMaterial.setAlbTexIndex(0);
    defaultMaterial.setNrmlTexIndex(0);

    defaultMaterialHandle = tinyFS->addToRegistry(defaultMaterial).handle;
}

TinyHandle TinyProject::addSceneFromModel(TinyModel& model, TinyHandle parentFolder) {
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
    for (auto& texture : model.textures) {
        texture.vkCreate(deviceVK);

        // TinyHandle handle = registry->add(textureData).handle;
        TinyHandle fnHandle = tinyFS->addFile(fnTexFolder, texture.name, std::move(&texture));
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
    for (auto& mesh : model.meshes) {
        // Remap submeshes' material indices
        std::vector<TinySubmesh> remappedSubmeshes = mesh.submeshes;
        for (auto& submesh : remappedSubmeshes) {
            bool valid = isValidIndex(submesh.material.index, glmMatRHandle);
            submesh.material = valid ? glmMatRHandle[submesh.material.index] : TinyHandle();
        }

        mesh.vkCreate(deviceVK);

        mesh.setSubmeshes(remappedSubmeshes);

        TinyHandle fnHandle = tinyFS->addFile(fnMeshFolder, mesh.name, std::move(&mesh));
        TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

        glbMeshRHandle.push_back(tHandle.handle);
    }

    // Import skeletons to registry
    std::vector<TinyHandle> glbSkeleRHandle;
    for (auto& skeleton : model.skeletons) {
        // TinyHandle handle = registry->add(rSkeleton).handle;
        TinyHandle fnHandle = tinyFS->addFile(fnSkeleFolder, skeleton.name, std::move(&skeleton));
        TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

        glbSkeleRHandle.push_back(tHandle.handle);
    }

    // Create scene with nodes - preserve hierarchy but remap resource references
    TinyScene scene;
    scene.name = model.name;

    // First pass: Insert all nodes and collect their actual handles
    std::vector<TinyHandle> nodeHandles;
    nodeHandles.reserve(model.nodes.size());

    for (const auto& node : model.nodes) {
        TinyNode rtNode = node; // Copy node data

        // Remap MeshRender component's mesh reference
        if (rtNode.hasType(NTypes::MeshRender)) {
            auto* meshRender = rtNode.get<TinyNode::MeshRender>();
            if (meshRender) meshRender->meshHandle = glbMeshRHandle[meshRender->meshHandle.index];
        }

        // Remap Skeleton component's registry reference
        if (rtNode.hasType(NTypes::Skeleton)) {
            auto* skeleton = rtNode.get<TinyNode::Skeleton>();
            if (skeleton) {
                skeleton->skeleHandle = glbSkeleRHandle[skeleton->skeleHandle.index];

                // Construct the final bone transforms array (set to identity, resolve through skeleton later)
                const auto& skeleData = registryRef().get<TinySkeleton>(skeleton->skeleHandle);
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
        TinyNode* rtNode = scene.nodes.get(nodeHandles[i]);
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

    // Add scene to registry
    TinyHandle fnHandle = tinyFS->addFile(fnModelFolder, scene.name, &scene);
    TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

    // Return the model folder handle instead of the scene handle
    return fnModelFolder;
}



void TinyProject::addSceneInstance(TinyHandle sceneHandle, TinyHandle parentNode) {
    const auto& registry = registryRef();
    const TinyScene* sourceScene = registry.get<TinyScene>(sceneHandle);
    TinyScene* activeScene = getActiveScene();
    
    if (!sourceScene || !activeScene) return;

    // Use root node if no valid parent provided
    TinyHandle actualParent = parentNode.valid() ? parentNode : activeScene->rootHandle;
    
    // Use the scene's addScene method to merge the source scene into active scene
    activeScene->addScene(*sourceScene, actualParent);
    
    // Update transforms for the entire active scene
    activeScene->updateGlbTransform();
}

bool TinyProject::setActiveScene(TinyHandle sceneHandle) {
    // Verify the handle points to a valid TinyScene in the registry
    const TinyScene* scene = registryRef().get<TinyScene>(sceneHandle);
    if (!scene) {
        return false; // Invalid handle or not a scene
    }
    
    // Switch the active scene
    activeSceneHandle = sceneHandle;

    // Update transforms for the new active scene
    TinyScene* activeScene = getActiveScene();
    if (activeScene) activeScene->updateGlbTransform();
    
    return true;
}


void TinyProject::renderNodeTreeImGui(TinyHandle nodeHandle, int depth) {
    TinyScene* activeScene = getActiveScene();
    if (!activeScene) return;
    
    // Use root node if no valid handle provided
    if (!nodeHandle.valid()) nodeHandle = activeScene->rootHandle;

    const TinyNode* node = activeScene->nodes.get(nodeHandle);
    if (!node) return;
    
    // Create a unique ID for this node
    ImGui::PushID(static_cast<int>(nodeHandle.index));
    
    // Check if this node has children
    bool hasChildren = !node->childrenHandles.empty();
    bool isSelected = (selectedHandle.isScene() && selectedHandle.handle.index == nodeHandle.index && selectedHandle.handle.version == nodeHandle.version);
    bool isHeld = (heldHandle.isScene() && heldHandle.handle.index == nodeHandle.index && heldHandle.handle.version == nodeHandle.version);
    
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
        holdSceneNode(nodeHandle);  // Set the held node during drag
        
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
            selectSceneNode(nodeHandle);
        }
        
        // Reset drag delta for next interaction
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
    }
    
    // Select node on right-click (immediate selection before context menu)
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        selectSceneNode(nodeHandle);
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
                selectSceneNode(draggedNode);
                
                // Clear held state after successful drop
                clearHeld();
            }
        }
        
        // Accept scene drops (from filesystem nodes)
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_FNODE")) {
            TinyHandle sceneFNodeHandle = *(const TinyHandle*)payload->Data;
            
            // Get the filesystem node to access its TypeHandle
            const TinyFS::Node* sceneFile = tinyFS->getFNodes().get(sceneFNodeHandle);
            if (sceneFile && sceneFile->isFile() && sceneFile->tHandle.isType<TinyScene>()) {
                // Extract the registry handle from the TypeHandle
                TinyHandle sceneRegistryHandle = sceneFile->tHandle.handle;
                
                // Verify the scene exists and instantiate it at this node
                const TinyScene* scene = registryRef().get<TinyScene>(sceneRegistryHandle);
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
            if (fileNode && fileNode->isFile() && fileNode->tHandle.isType<TinyScene>()) {
                // This is a scene file - instantiate it at this node
                TinyHandle sceneRegistryHandle = fileNode->tHandle.handle;
                
                // Safety check: prevent dropping a scene into itself
                if (sceneRegistryHandle == activeSceneHandle) {
                    // Cannot drop active scene into itself - ignore the operation
                    ImGui::SetTooltip("Cannot drop a scene into itself!");
                } else {
                    // Verify the scene exists and instantiate it at this node
                    const TinyScene* scene = registryRef().get<TinyScene>(sceneRegistryHandle);
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
        
        if (ImGui::MenuItem("Add Child")) {
            TinyScene* scene = getActiveScene();
            if (scene) {
                TinyHandle newNodeHandle = scene->addNode("New Node", nodeHandle);
                selectSceneNode(newNodeHandle);
                expandNode(nodeHandle); // Expand parent to show new child
            }
        }
        
        ImGui::Separator();

        bool isRootNode = (nodeHandle == activeScene->rootHandle);
        if (ImGui::MenuItem("Delete", nullptr, false, !isRootNode)) {
            TinyScene* scene = getActiveScene();
            if (scene && scene->removeNode(nodeHandle)) {
                clearSelection(); // Clear selection
            }
        }

        if (ImGui::MenuItem("Flatten", nullptr, false, !isRootNode && hasChildren)) {
            TinyScene* scene = getActiveScene();
            TinyHandle parentHandle = node->parentHandle;
            if (scene && scene->flattenNode(nodeHandle)) {
                selectSceneNode(parentHandle); // Select the parent node after flattening
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
            const TinyNode* nodeA = activeScene->nodes.get(a);
            const TinyNode* nodeB = activeScene->nodes.get(b);
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
    TinyScene* activeScene = getActiveScene();
    if (!activeScene) return;
    
    // Get the target node
    const TinyNode* targetNode = activeScene->nodes.get(nodeHandle);
    if (!targetNode) return;
    
    // Walk up the parent chain and expand all parents
    TinyHandle currentHandle = targetNode->parentHandle;
    while (currentHandle.valid()) {
        expandedNodes.insert(currentHandle);
        
        const TinyNode* currentNode = activeScene->nodes.get(currentHandle);
        if (!currentNode) break;
        
        currentHandle = currentNode->parentHandle;
    }
    
    // Also expand the target node itself if it has children
    if (!targetNode->childrenHandles.empty()) {
        expandedNodes.insert(nodeHandle);
    }
}

void TinyProject::expandFNodeParentChain(TinyHandle fNodeHandle) {
    // Get the target file node
    const TinyFS::Node* targetFNode = tinyFS->getFNodes().get(fNodeHandle);
    if (!targetFNode) return;
    
    // Walk up the parent chain and expand all parents
    TinyHandle currentHandle = targetFNode->parent;
    while (currentHandle.valid()) {
        expandedFNodes.insert(currentHandle);
        
        const TinyFS::Node* currentFNode = tinyFS->getFNodes().get(currentHandle);
        if (!currentFNode) break;
        
        currentHandle = currentFNode->parent;
    }
    
    // Also expand the target node itself if it's a folder with children
    if (!targetFNode->isFile() && !targetFNode->children.empty()) {
        expandedFNodes.insert(fNodeHandle);
    }
}



void TinyProject::processPendingDeletions() {
    if (pendingFNodeDeletions.empty()) return;
    
    TinyFS& fs = filesystem();
    
    for (TinyHandle handle : pendingFNodeDeletions) {
        fs.removeFNode(handle);
        
        // Clear selection if the selected file was deleted
        if (selectedHandle.isFile() && selectedHandle.handle == handle) {
            clearSelection();
        }
    }

    pendingFNodeDeletions.clear();
}

void TinyProject::selectFileNode(TinyHandle fileHandle) {
    // Check if the file handle is valid
    if (!fileHandle.valid()) {
        clearSelection();
        return;
    }
    
    // Get the filesystem node
    TinyFS& fs = filesystem();
    const TinyFS::Node* node = fs.getFNodes().get(fileHandle);
    if (!node) {
        // Invalid node, clear selection
        clearSelection();
        return;
    }
    
    // Only select if it's an actual file, not a folder
    if (node->isFile()) {
        selectedHandle = SelectHandle(fileHandle, SelectHandle::Type::File);
    }
    // If it's a folder, ignore the selection (don't set selectedHandle)
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
    bool isSelected = (selectedHandle.isFile() && selectedHandle.handle.index == nodeHandle.index && selectedHandle.handle.version == nodeHandle.version);
    
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
        
        // Force open if this node is in the expanded set
        bool forceOpen = isFNodeExpanded(nodeHandle);
        
        // Set the default open state (this will be overridden by user interaction)
        if (forceOpen) {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        }
        
        bool nodeOpen = ImGui::TreeNodeEx(displayName.c_str(), flags);
        
        // Track expansion state changes (only for folders with children)
        if (hasChildren) {
            if (nodeOpen && !forceOpen) {
                // User expanded this folder manually
                expandedFNodes.insert(nodeHandle);
            } else if (!nodeOpen && isFNodeExpanded(nodeHandle)) {
                // User collapsed this folder manually
                expandedFNodes.erase(nodeHandle);
            }
        }
        
        // Pop the style colors for folders right after TreeNodeEx
        ImGui::PopStyleColor(2);
        
        // Handle selection - only select on mouse release if we didn't drag
        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            // Check if this was a click (no significant drag distance)
            ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
            float dragDistance = sqrtf(dragDelta.x * dragDelta.x + dragDelta.y * dragDelta.y);
            
            // Only select if drag distance was minimal (treat as click)
            if (dragDistance < 5.0f) { // 5 pixel tolerance
                selectFileNode(nodeHandle);
            }
            
            // Reset drag delta for next interaction
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        }
        
        // Select folder on right-click (immediate selection before context menu)
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            selectFileNode(nodeHandle);
        }
        
        // Drag source for folders
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            // Set this folder as held when dragging begins
            holdFileNode(nodeHandle);
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
                selectFileNode(newFolderHandle);
                // Expand parent chain to show the new folder
                expandFNodeParentChain(newFolderHandle);
            }

            if (ImGui::MenuItem("Add Scene")) {
                TinyScene newScene;
                newScene.name = "New Scene";
                newScene.addRoot("Root");
                
                // Add scene to registry and create file node
                TinyScene* scenePtr = &newScene;
                TinyHandle fileHandle = fs.addFile(nodeHandle, "New Scene", scenePtr);
                selectFileNode(fileHandle);
                // Expand parent chain to show the new scene
                expandFNodeParentChain(fileHandle);
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Delete", nullptr, false, node->deletable())) {
                if (node->deletable()) queueFNodeForDeletion(nodeHandle);
            }

            // Folder operations don't affect file, no need for pending deletion
            if (ImGui::MenuItem("Flatten", nullptr, false, node->deletable())) {
                if (node->deletable()) fs.flattenFNode(nodeHandle);
            }

            ImGui::Separator();
            
            if (ImGui::MenuItem("Load Model...")) {
                // Start from current working directory for full file system access
                std::filesystem::path startPath = std::filesystem::current_path();
                g_fileDialog.open(startPath, nodeHandle);
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
        if (node->tHandle.isType<TinyScene>()) {
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
                // Don't select immediately - let the drag detection handle it below
            }
            
            // Handle selection - only select on mouse release if we didn't drag
            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                // Check if this was a click (no significant drag distance)
                ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
                float dragDistance = sqrtf(dragDelta.x * dragDelta.x + dragDelta.y * dragDelta.y);
                
                // Only select if drag distance was minimal (treat as click)
                if (dragDistance < 5.0f) { // 5 pixel tolerance
                    selectFileNode(nodeHandle);
                }
                
                // Reset drag delta for next interaction
                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            }
            
            // Select scene file on right-click (immediate selection before context menu)
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                selectFileNode(nodeHandle);
            }
            
            // Drag source for scene files
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                // Set this scene file as held when dragging begins
                holdFileNode(nodeHandle);
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
                            selectSceneNode(getRootNodeHandle()); // Reset node selection to root
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
            
            // Handle selection - only select on mouse release if we didn't drag
            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                // Check if this was a click (no significant drag distance)
                ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
                float dragDistance = sqrtf(dragDelta.x * dragDelta.x + dragDelta.y * dragDelta.y);
                
                // Only select if drag distance was minimal (treat as click)
                if (dragDistance < 5.0f) { // 5 pixel tolerance
                    selectFileNode(nodeHandle);
                }
                
                // Reset drag delta for next interaction
                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            }
            
            // Select file on right-click (immediate selection before context menu)
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                selectFileNode(nodeHandle);
            }
            
            // Drag source for other files
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                // Set this file as held when dragging begins
                holdFileNode(nodeHandle);
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
    
    // Clear held handle if no drag operation is active
    if (heldHandle.valid() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        clearHeld();
    }
    
    ImGui::PopID();
}

void TinyProject::renderFileDialog() {
    // Update the dialog state first
    g_fileDialog.update();
    
    // Only open the popup once when first requested, right before trying to begin it
    if (g_fileDialog.justOpened && !ImGui::IsPopupOpen("Load Model File")) {
        ImGui::OpenPopup("Load Model File");
        g_fileDialog.justOpened = false;
    }
    
    // Use a local variable for the open state to avoid ImGui modifying our state directly
    bool modalOpen = g_fileDialog.isOpen && !g_fileDialog.shouldClose;
    if (ImGui::BeginPopupModal("Load Model File", &modalOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        // Current path display
        ImGui::Text("Path: %s", g_fileDialog.currentPath.string().c_str());
        
        ImGui::Separator();
        
        // File list
        ImGui::BeginChild("FileList", ImVec2(600, 400), true);
        
        // Parent directory button
        if (g_fileDialog.currentPath.has_parent_path()) {
            if (ImGui::Selectable(".. (Parent Directory)", false)) {
                g_fileDialog.currentPath = g_fileDialog.currentPath.parent_path();
                g_fileDialog.refreshFileList();
                g_fileDialog.selectedFile.clear();
            }
        }
        
        // File and directory listing
        for (const auto& entry : g_fileDialog.currentFiles) {
            std::string fileName = entry.path().filename().string();
            bool isDirectory = entry.is_directory();
            bool isModelFile = !isDirectory && g_fileDialog.isModelFile(entry.path());
            
            // Color coding: directories in blue, model files in green, others in gray
            if (isDirectory) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f)); // Light blue
                fileName = "[DIR] " + fileName;
            } else if (isModelFile) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 1.0f, 0.5f, 1.0f)); // Light green
                fileName = "[MDL] " + fileName;
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f)); // Gray
                fileName = "[FILE] " + fileName;
            }
            
            bool isSelected = (g_fileDialog.selectedFile == entry.path().string());
            
            if (ImGui::Selectable(fileName.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (isDirectory) {
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        // Navigate into directory
                        g_fileDialog.currentPath = entry.path();
                        g_fileDialog.refreshFileList();
                        g_fileDialog.selectedFile.clear();
                    }
                } else if (isModelFile) {
                    g_fileDialog.selectedFile = entry.path().string();
                    
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        // Double-click to load model
                        loadModelFromPath(entry.path().string(), g_fileDialog.targetFolder);
                        g_fileDialog.close();
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            
            ImGui::PopStyleColor();
        }
        
        ImGui::EndChild();
        
        ImGui::Separator();
        
        // Selected file display
        if (!g_fileDialog.selectedFile.empty()) {
            std::filesystem::path selectedPath(g_fileDialog.selectedFile);
            ImGui::Text("Selected: %s", selectedPath.filename().string().c_str());
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No file selected");
        }
        
        ImGui::Separator();
        
        // Action buttons
        bool canLoad = !g_fileDialog.selectedFile.empty() && 
                      g_fileDialog.isModelFile(std::filesystem::path(g_fileDialog.selectedFile));
        
        if (ImGui::Button("Load", ImVec2(120, 0))) {
            if (canLoad) {
                loadModelFromPath(g_fileDialog.selectedFile, g_fileDialog.targetFolder);
                g_fileDialog.close();
                ImGui::CloseCurrentPopup();
            }
        }
        
        if (!canLoad) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "Please select a .glb or .gltf file");
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            g_fileDialog.close();
            ImGui::CloseCurrentPopup();
        }
        
        // Handle close button (X) or ESC key
        if (!modalOpen) {
            g_fileDialog.close();
        }

        ImGui::EndPopup();
    }
}

void TinyProject::loadModelFromPath(const std::string& filePath, TinyHandle targetFolder) {
    try {
        // Load the model using TinyLoader
        TinyModel model = TinyLoader::loadModel(filePath);
        
        // Add the model to the project in the specified folder (returns model folder handle)
        TinyHandle modelFolderHandle = addSceneFromModel(model, targetFolder);
        
        if (modelFolderHandle.valid()) {
            // Success! The model has been loaded and added to the project
            
            // Select the newly created model folder
            selectFileNode(modelFolderHandle);
            
            // Expand the target folder to show the newly imported model folder
            expandedFNodes.insert(targetFolder);
            
            // Also expand the parent chain of the target folder to ensure it's visible
            expandFNodeParentChain(targetFolder);
            
            printf("Successfully loaded model: %s\n", filePath.c_str());
        } else {
            printf("Failed to add model to project: %s\n", filePath.c_str());
        }
        
    } catch (const std::exception& e) {
        printf("Error loading model %s: %s\n", filePath.c_str(), e.what());
    }
}

