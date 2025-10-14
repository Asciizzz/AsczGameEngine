#include "TinyEngine/TinyProject.hpp"
#include <imgui.h>

using NTypes = TinyNode::Types;

// A quick function for range validation
template<typename T>
bool isValidIndex(int index, const std::vector<T>& vec) {
    return index >= 0 && index < static_cast<int>(vec.size());
}


TinyProject::TinyProject(const TinyVK::Device* deviceVK) : deviceVK(deviceVK) {
    registry = MakeUnique<TinyRegistry>();

    // Create root runtime node
    auto rootNode = MakeUnique<TinyNodeRT>();
    rootNode->name = "Root";
    // Children will be added upon scene population

    rtNodes.push_back(std::move(rootNode));

    // Create camera and global UBO manager
    tinyCamera = MakeUnique<TinyCamera>(glm::vec3(0.0f, 0.0f, 0.0f), 45.0f, 0.1f, 100.0f);
    tinyGlobal = MakeUnique<TinyGlobal>(2);
    tinyGlobal->createVkResources(deviceVK);
}

TinyHandle TinyProject::addSceneFromModel(const TinyModel& model) {
    // Import textures to registry
    std::vector<TinyHandle> glbTexrHandle;
    for (const auto& texture : model.textures) {
        TinyRTexture textureData;
        textureData.import(deviceVK, texture);

        TinyHandle handle = registry->add(textureData);
        glbTexrHandle.push_back(handle);
    }

    // Import materials to registry with remapped texture references
    std::vector<TinyHandle> glbMatrHandle;
    for (const auto& material : model.materials) {
        TinyRMaterial correctMat;

        // Remap the material's texture indices
        uint32_t localAlbIndex = material.localAlbTexture;
        bool localAlbValid = localAlbIndex >= 0 && localAlbIndex < static_cast<int>(glbTexrHandle.size());
        correctMat.setAlbTexIndex(localAlbValid ? glbTexrHandle[localAlbIndex].index : 0);

        uint32_t localNrmlIndex = material.localNrmlTexture;
        bool localNrmlValid = localNrmlIndex >= 0 && localNrmlIndex < static_cast<int>(glbTexrHandle.size());
        correctMat.setNrmlTexIndex(localNrmlValid ? glbTexrHandle[localNrmlIndex].index : 0);

        TinyHandle handle = registry->add(correctMat);
        glbMatrHandle.push_back(handle);
    }

    // Import meshes to registry with remapped material references
    std::vector<TinyHandle> glbMeshrHandle;
    for (const auto& mesh : model.meshes) {
        TinyRMesh meshData;
        meshData.import(deviceVK, mesh);

        // Remap submeshes' material indices
        std::vector<TinySubmesh> remappedSubmeshes = mesh.submeshes;
        for (auto& submesh : remappedSubmeshes) {
            bool valid = isValidIndex(submesh.materialIndex, glbMatrHandle);
            submesh.materialIndex = valid ? glbMatrHandle[submesh.materialIndex].index : -1;
        }

        meshData.setSubmeshes(remappedSubmeshes);

        TinyHandle handle = registry->add(meshData);
        glbMeshrHandle.push_back(handle);
    }

    // Import skeletons to registry
    std::vector<TinyHandle> glbSkelerHandle;
    for (const auto& skeleton : model.skeletons) {
        TinyRSkeleton rSkeleton;
        rSkeleton.bones = skeleton.bones;

        TinyHandle handle = registry->add(rSkeleton);
        glbSkelerHandle.push_back(handle);
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
    return registry->add(scene);
}



void TinyProject::addSceneInstance(TinyHandle sceneHandle, uint32_t rootIndex, glm::mat4 at) {
    const TinyRScene* scene = registry->get<TinyRScene>(sceneHandle);
    if (!scene || !scene->hasNodes()) {
        printf("Error: Invalid scene handle %llu or empty scene\n", sceneHandle.value);
        return;
    }

    uint32_t rtRootIndex = rootIndex < rtNodes.size() ? rootIndex : 0;
    uint32_t startRuntimeIndex = rtNodes.size(); // First runtime node index for this instance
    
    // Create mapping from scene node index to runtime node index
    UnorderedMap<int32_t, uint32_t> sceneToRuntimeMap;

    // First pass: Create all runtime nodes and copy scene node data
    for (int32_t i = 0; i < static_cast<int32_t>(scene->nodes.size()); ++i) {
        const TinyNode& sceneNode = scene->nodes[i];
        
        auto rtNode = MakeUnique<TinyNodeRT>();
        rtNode->copyFromSceneNode(sceneNode);
        
        uint32_t rtIndex = rtNodes.size();
        sceneToRuntimeMap[i] = rtIndex;
        rtNodes.push_back(std::move(rtNode));
    }

    // Second pass: Remap all references (parent/children + component skeleton references)
    for (int32_t i = 0; i < static_cast<int32_t>(scene->nodes.size()); ++i) {
        const TinyNode& sceneNode = scene->nodes[i];
        uint32_t rtIndex = sceneToRuntimeMap[i];
        auto& rtNode = rtNodes[rtIndex];

        // Remap parent-child relationships
        if (sceneNode.parent.isValid() && sceneNode.parent.index < scene->nodes.size()) {
            // Has parent within scene - remap to runtime parent
            uint32_t rtParentIndex = sceneToRuntimeMap[sceneNode.parent.index];
            rtNode->parentIdx = rtParentIndex;
            rtNodes[rtParentIndex]->addChild(rtIndex, rtNodes);
        } else {
            // No parent in scene - attach to project root
            rtNode->parentIdx = rtRootIndex;
            rtNodes[rtRootIndex]->addChild(rtIndex, rtNodes);
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
                if (sceneMeshRender && sceneMeshRender->skeleNode.isValid() && 
                    sceneMeshRender->skeleNode.index < scene->nodes.size()) {
                    meshRender->skeleNodeRT = sceneToRuntimeMap[sceneMeshRender->skeleNode.index];
                }
                rtMeshRenderIdxs.push_back(rtIndex);
            }
        }

        if (rtNode->hasType(NTypes::BoneAttach)) {
            auto* boneAttach = rtNode->get<TinyNodeRT::BoneAttach>();
            if (boneAttach) {
                const auto* sceneBoneAttach = sceneNode.get<TinyNode::BoneAttach>();
                if (sceneBoneAttach && sceneBoneAttach->skeleNode.isValid() && 
                    sceneBoneAttach->skeleNode.index < scene->nodes.size()) {
                    boneAttach->skeleNodeRT = sceneToRuntimeMap[sceneBoneAttach->skeleNode.index];
                }
            }
        }

        if (rtNode->hasType(NTypes::Skeleton)) {
            auto* skeleton = rtNode->get<TinyNodeRT::Skeleton>();
            if (skeleton) {
                const auto* skeleData = registry->get<TinyRSkeleton>(skeleton->skeleRegistry);
                if (skeleData) {
                    skeleton->boneTransformsFinal.resize(skeleData->bones.size(), glm::mat4(1.0f));
                }
            }
        }
    }

    // Update transforms for the entire hierarchy
    updateGlobalTransforms(0);
}

void TinyProject::printRuntimeNodeRecursive(
    const UniquePtrVec<TinyNodeRT>& rtNodes,
    TinyRegistry* registry,
    const TinyHandle& runtimeHandle,
    int depth
) {
    const TinyNodeRT* rtNode = rtNodes[runtimeHandle.index].get();

    // Format runtime index padded to 3 chars
    char idxBuf[8];
    snprintf(idxBuf, sizeof(idxBuf), "%3u", runtimeHandle.index);

    // Indentation
    std::string indent(depth * 2, ' ');

    // Extract local and global positions
    glm::vec3 localPos = glm::vec3(rtNode->localTransform[3]); // Local transform translation
    glm::vec3 globalPos = glm::vec3(rtNode->globalTransform[3]); // Global translation

    // Print: [idx] indent Name (Parent: parentIndex) -> Local (x, y, z) - Glb (x, y, z)
    printf("[%s] %s%s (Parent: %u) -> Local (%.2f, %.2f, %.2f) - Glb (%.2f, %.2f, %.2f)\n",
        idxBuf,
        indent.c_str(),
        rtNode->name.c_str(),
        rtNode->parentIdx,
        localPos.x, localPos.y, localPos.z,
        globalPos.x, globalPos.y, globalPos.z);

    // Recurse for children
    for (uint32_t childIdx : rtNode->childrenIdxs) {
        TinyHandle childHandle = TinyHandle(childIdx);
        printRuntimeNodeRecursive(rtNodes, registry, childHandle, depth + 1);
    }
}

void TinyProject::printRuntimeNodeOrdered() {
    for (size_t i = 0; i < rtNodes.size(); ++i) {
        const UniquePtr<TinyNodeRT>& runtimeNode = rtNodes[i];
        if (!runtimeNode) continue;

        // Parent index
        uint32_t parentIndex = runtimeNode->parentIdx;

        // Print in ordered flat list
        printf("[%3zu] RuntimeNode_%zu (Parent: %u)\n",
            i,
            i,
            parentIndex);
    }
}

void TinyProject::updateGlobalTransforms(uint32_t rootNodeIndex, const glm::mat4& parentGlobalTransform) {
    // Validate the node index
    if (rootNodeIndex == UINT32_MAX || rootNodeIndex >= rtNodes.size()) {
        return;
    }

    UniquePtr<TinyNodeRT>& runtimeNode = rtNodes[rootNodeIndex];
    if (!runtimeNode) return;

    // Use the local transform which contains the original scene node transform + any instance overrides
    glm::mat4 localTransform = runtimeNode->localTransform;

    // Calculate global transform: parent global * local transform
    runtimeNode->globalTransform = parentGlobalTransform * localTransform;

    // Mark this node as clean
    runtimeNode->isDirty = false;

    // Recursively update all children
    for (uint32_t childIdx : runtimeNode->childrenIdxs) {
        updateGlobalTransforms(childIdx, runtimeNode->globalTransform);
    }
}

// TinyNodeRT method implementation
void TinyNodeRT::addChild(uint32_t childIndex, std::vector<std::unique_ptr<TinyNodeRT>>& allrtNodes) {
    // Add child to this node's children list
    childrenIdxs.push_back(childIndex);
    
    // Mark this node (parent) as dirty
    isDirty = true;
    
    // Mark the child node as dirty (parent will be set separately by the caller)
    if (childIndex < allrtNodes.size() && allrtNodes[childIndex]) {
        allrtNodes[childIndex]->isDirty = true;
    }
}

void TinyProject::runPlayground(float dTime) {
    return;

    // Get the root node (index 0)
    TinyNodeRT* node0 = rtNodes[1].get();
    TinyNodeRT* node1 = rtNodes[10].get();

    // Calculate rotation: 90 degrees per second = π/2 radians per second
    float rotationSpeed = glm::radians(90.0f); // 90 degrees per second in radians
    float rotationThisFrame = rotationSpeed * dTime;
    
    // Create rotation matrix around Y axis
    glm::mat4 rotMat0 = glm::rotate(glm::mat4(1.0f), rotationThisFrame, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 rotMat1 = glm::rotate(glm::mat4(1.0f), -rotationThisFrame, glm::vec3(0.0f, 1.0f, 0.0f));
    
    // Apply rotation to the local transform
    node0->localTransform = rotMat0 * node0->localTransform;
    node1->localTransform = rotMat1 * node1->localTransform;

    // Update the entire transform hierarchy every frame (no dirty flag checking)
    updateGlobalTransforms(0);
}





void TinyProject::renderNodeTreeImGui(uint32_t nodeIndex, int depth) {
    if (nodeIndex >= rtNodes.size() || !rtNodes[nodeIndex]) {
        return;
    }

    const auto& node = rtNodes[nodeIndex];
    
    // Create a unique ID for this node
    ImGui::PushID(static_cast<int>(nodeIndex));
    
    // Check if this node has children
    bool hasChildren = !node->childrenIdxs.empty();
    
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
        ImGui::Text("Index: %u", nodeIndex);
        ImGui::Text("Parent: %u", node->parentIdx);
        ImGui::Text("Children: %zu", node->childrenIdxs.size());
        ImGui::Text("World Position: (%.2f, %.2f, %.2f)", worldPos.x, worldPos.y, worldPos.z);
        ImGui::Text("Type Mask: 0x%X", node->types);
        ImGui::EndTooltip();
    }
    
    // If node is open and has children, recurse for children
    if (nodeOpen && hasChildren) {
        for (uint32_t childIdx : node->childrenIdxs) {
            renderNodeTreeImGui(childIdx, depth + 1);
        }
        ImGui::TreePop();
    }
    
    ImGui::PopID();
}

// Scene deletion can be handled through registry cleanup
// Runtime nodes are managed through the rtNodes vector directly