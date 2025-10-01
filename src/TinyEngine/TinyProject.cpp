#include "TinyEngine/TinyProject.hpp"

using HType = TinyHandle::Type;
using NTypes = TinyNode::Types;

// A quick function for range validation
template<typename T>
bool isValidIndex(int index, const std::vector<T>& vec) {
    return index >= 0 && index < static_cast<int>(vec.size());
}

using RNode = TinyRegistry::RNode;


uint32_t TinyProject::addTemplateFromModel(const TinyModelNew& model) {
    std::vector<TinyHandle> glbMeshRegHandle; // Ensure correct mapping
    for (const auto& mesh : model.meshes) {
        TinyHandle handle = registry->addMesh(mesh);
        glbMeshRegHandle.push_back(handle);
    }

    std::vector<TinyHandle> glbTexRegHandle;
    for (const auto& texture : model.textures) {
        TinyHandle handle = registry->addTexture(texture);
        glbTexRegHandle.push_back(handle);
    }

    std::vector<TinyHandle> glbMatRegHandle;
    for (const auto& material : model.materials) {
        TinyRegistry::RMaterial correctMat;

        // Remap the material's texture indices
        
        uint32_t localAlbIndex = material.localAlbTexture;
        bool localAlbValid = localAlbIndex >= 0 && localAlbIndex < static_cast<int>(glbTexRegHandle.size());
        correctMat.setAlbTexIndex(localAlbValid ? glbTexRegHandle[localAlbIndex].index : 0);

        uint32_t localNrmlIndex = material.localNrmlTexture;
        bool localNrmlValid = localNrmlIndex >= 0 && localNrmlIndex < static_cast<int>(glbTexRegHandle.size());
        correctMat.setNrmlTexIndex(localNrmlValid ? glbTexRegHandle[localNrmlIndex].index : 0);

        TinyHandle handle = registry->addMaterial(correctMat);
        glbMatRegHandle.push_back(handle);
    }

    std::vector<TinyHandle> glbSkeleRegHandle;
    for (const auto& skeleton : model.skeletons) {
        TinyRegistry::RSkeleton rSkeleton;
        rSkeleton.bones = skeleton.construct();

        TinyHandle handle = registry->addSkeleton(rSkeleton);
        glbSkeleRegHandle.push_back(handle);
    }

    std::vector<TinyHandle> glbNodeRegHandle;

    UnorderedMap<int, TinyHandle> localNodeIndexToGlobalNodeHandle; // Left: local index, Right: global index

    for (int i = 0; i < static_cast<int>(model.nodes.size()); ++i) {
        // Just occupy the index
        TinyHandle handle = registry->addNode(RNode());
        localNodeIndexToGlobalNodeHandle[i] = handle;

        glbNodeRegHandle.push_back(handle);
    }

    for (int i = 0; i < static_cast<int>(model.nodes.size()); ++i) {
        const TinyNode& localNode = model.nodes[i];
        TinyNode& regNode = *registry->getNodeData(glbNodeRegHandle[i]);
        regNode.scope = localNode.scope;
        regNode.name = localNode.name;

        // Remapping children and parent
        regNode.parent = localNodeIndexToGlobalNodeHandle[localNode.parent.index];
        regNode.children.clear();
        for (const TinyHandle& localChild : localNode.children) {
            regNode.children.push_back(localNodeIndexToGlobalNodeHandle[localChild.index]);
        }

    // Add components

        if (localNode.hasType(NTypes::Node)) {
            regNode.transform = localNode.transform;
        }

        if (localNode.hasType(NTypes::MeshRender)) {
            const auto& mesh3D = localNode.get<TinyNode::MeshRender>();
            TinyNode::MeshRender trueMesh3D;

            trueMesh3D.mesh = glbMeshRegHandle[mesh3D->mesh.index];
            for (TinyHandle localMat : mesh3D->submeshMats) {
                bool valid = isValidIndex(localMat.index, glbMatRegHandle);
                trueMesh3D.submeshMats.push_back(valid ? glbMatRegHandle[localMat.index] : TinyHandle::invalid());
            }

            bool hasValidSkeleton = isValidIndex(mesh3D->skeleNode.index, glbNodeRegHandle);
            bool isInNodeMap = localNodeIndexToGlobalNodeHandle.count(mesh3D->skeleNode.index) > 0;
            trueMesh3D.skeleNode = (hasValidSkeleton && isInNodeMap)
                ? localNodeIndexToGlobalNodeHandle[mesh3D->skeleNode.index]
                : TinyHandle::invalid();

            regNode.add(trueMesh3D);
        }

        if (localNode.hasType(NTypes::Skeleton)) {
            const auto& skele3D = localNode.get<TinyNode::Skeleton>();
            TinyNode::Skeleton trueSkel3D;

            bool hasValidSkeleton = isValidIndex(skele3D->skeleRegistry.index, glbSkeleRegHandle);
            trueSkel3D.skeleRegistry = glbSkeleRegHandle[skele3D->skeleRegistry.index];

            regNode.add(trueSkel3D);
        }
    }

    templates.push_back(TinyTemplate(glbNodeRegHandle));
    return static_cast<uint32_t>(templates.size() - 1);
}



// Recursive function to construct runtime node tree

void TinyProject::addNodeInstance(uint32_t templateIndex, uint32_t rootIndex, glm::mat4 at) {
    if (!isValidIndex(templateIndex, templates)) {
        printf("Error: Invalid template index %d\n", templateIndex);
        return;
    }

    UnorderedMap<TinyHandle, uint32_t> regHandleToRtNodeIndex;

    const TinyTemplate& temp = templates[templateIndex];

    // First pass: create and append - each template instance gets its own runtime nodes
    for (const TinyHandle& regHandle : temp.registryNodes) {
        const TinyNode* regNode = registry->getNodeData(regHandle);
        if (!regNode) {
            printf("Warning: Registry node handle %llu is invalid, skipping.\n", regHandle.value);
            continue;
        }

        auto rtNode = MakeUnique<TinyNodeRT3D>();
        rtNode->regHandle = regHandle;
        rtNode->regHandle.owned = false; // Runtime node owned nothing

        regHandleToRtNodeIndex[regHandle] = rtNodes.size();
        rtNodes.push_back(std::move(rtNode));
    }

    // Second pass: set up the node (parent - child relationships, data, overrides)

    uint32_t rtRootIndex = rootIndex < rtNodes.size() ? rootIndex : 0;

    for (uint32_t i = 0; i < static_cast<uint32_t>(temp.registryNodes.size()); ++i) {
        const TinyHandle& regHandle = temp.registryNodes[i];
        const TinyNode* regNode = registry->getNodeData(regHandle);

        uint32_t rtNodeIndex = regHandleToRtNodeIndex[regHandle];
        auto& rtNode = rtNodes[rtNodeIndex];

        // Remap children and parent (do not affect the children OR the parent, only apply to this current node)
        TinyHandle regParentHandle = regNode->parent;

        // Default to root if no parent found
        bool hasParent = regParentHandle.isValid() && regHandleToRtNodeIndex.count(regParentHandle);
        uint32_t rtParentIndex = hasParent ? regHandleToRtNodeIndex[regParentHandle] : rtRootIndex;

        // If no parent, add child to root
        if (!hasParent) {
            rtNodes[rtRootIndex]->addChild(rtNodeIndex, rtNodes);
        }
        rtNode->parentIdx = rtParentIndex;

        for (const TinyHandle& regChildHandle : regNode->children) {
            if (regHandleToRtNodeIndex.count(regChildHandle)) {
                uint32_t rtChildIndex = regHandleToRtNodeIndex[regChildHandle];
                rtNode->addChild(rtChildIndex, rtNodes);
            }
        }

        // Construct components based on registry node data

        if (regNode->hasType(NTypes::Node)) {
            // Check if this node parent is the root node
            bool isRootChild = rtNode->parentIdx == rtRootIndex;

            glm::mat4 invRootTransform = glm::inverse(rtNodes[rtRootIndex]->globalTransform);
            rtNode->transformOverride = isRootChild ? at * invRootTransform : glm::mat4(1.0f);
        }

        if (regNode->hasType(NTypes::MeshRender)) {
            const auto& regMesh = regNode->get<TinyNode::MeshRender>();
            TinyNodeRT3D::MeshRender data;

            // Get the true skeleton node in the runtime
            bool hasValidSkeleNode = regMesh->skeleNode.isValid() && regHandleToRtNodeIndex.count(regMesh->skeleNode);
            data.skeleNodeRT = hasValidSkeleNode ? regHandleToRtNodeIndex[regMesh->skeleNode] : UINT32_MAX;

            rtNode->add(data);
        }

        if (regNode->hasType(NTypes::Skeleton)) {
            const auto& regSkeleton = regNode->get<TinyNode::Skeleton>();

            const auto* skeleData = registry->getSkeletonData(regSkeleton->skeleRegistry);

            TinyNodeRT3D::Skeleton data;
            data.boneTransformsFinal.resize( skeleData->bones.size(), glm::mat4(1.0f) );

            rtNode->add(data);
        }
    }

    // Mark the root node as dirty since it's receiving new children
    if (rtRootIndex < rtNodes.size() && rtNodes[rtRootIndex]) {
        rtNodes[rtRootIndex]->isDirty = true;
    }

    updateGlobalTransforms(0);
}

void TinyProject::printRuntimeNodeRecursive(
    const UniquePtrVec<TinyNodeRT3D>& rtNodes,
    TinyRegistry* registry,
    const TinyHandle& runtimeHandle,
    int depth
) {
    const TinyNodeRT3D* rtNode = rtNodes[runtimeHandle.index].get();
    const TinyNode* regNode = registry->getNodeData(rtNode->regHandle);

    // Format runtime index padded to 3 chars
    char idxBuf[8];
    snprintf(idxBuf, sizeof(idxBuf), "%3u", runtimeHandle.index);

    // Indentation
    std::string indent(depth * 2, ' ');

    // Extract local and global positions
    glm::vec3 localPos(0.0f);
    glm::vec3 globalPos(0.0f);
    
    if (regNode) {
        // Calculate local transform: registry transform * user override
        glm::mat4 localTransform = regNode->transform * rtNode->transformOverride;
        localPos = glm::vec3(localTransform[3]); // Extract translation component
    }
    
    globalPos = glm::vec3(rtNode->globalTransform[3]); // Extract global translation

    // Print: [idx] indent Name (Reg: regIndex, Parent: parentIndex) -> Local (x, y, z) - Glb (x, y, z)
    printf("[%s] %s%s (Reg: %u, Parent: %u) -> Local (%.2f, %.2f, %.2f) - Glb (%.2f, %.2f, %.2f)\n",
        idxBuf,
        indent.c_str(),
        regNode ? regNode->name.c_str() : "<invalid>",
        rtNode->regHandle.index,
        rtNode->parentIdx,
        localPos.x, localPos.y, localPos.z,
        globalPos.x, globalPos.y, globalPos.z);

    // Recurse for children
    for (uint32_t childIdx : rtNode->childrenIdxs) {
        TinyHandle childHandle = TinyHandle(childIdx, TinyHandle::Type::Node, false);
        printRuntimeNodeRecursive(rtNodes, registry, childHandle, depth + 1);
    }
}

void TinyProject::printRuntimeNodeOrdered() {
    for (size_t i = 0; i < rtNodes.size(); ++i) {
        const UniquePtr<TinyNodeRT3D>& runtimeNode = rtNodes[i];
        if (!runtimeNode) continue;

        // Lookup registry node name
        std::string regName = "???";
        if (TinyNode* regNode = registry->getNodeData(runtimeNode->regHandle)) {
            regName = regNode->name;
        }

        // Parent index
        uint32_t parentIndex = runtimeNode->parentIdx;

        // Print in ordered flat list
        printf("[%3zu] %s (Reg: %u, Parent: %u)\n",
            i,
            regName.c_str(),
            runtimeNode->regHandle.index,
            parentIndex);
    }
}

void TinyProject::updateGlobalTransforms(uint32_t rootNodeIndex, const glm::mat4& parentGlobalTransform) {
    // Validate the node index
    if (rootNodeIndex == UINT32_MAX || rootNodeIndex >= rtNodes.size()) {
        return;
    }

    UniquePtr<TinyNodeRT3D>& runtimeNode = rtNodes[rootNodeIndex];
    if (!runtimeNode) return;

    // Get the registry node to access the base transform
    const TinyNode* regNode = registry->getNodeData(runtimeNode->regHandle);
    if (!regNode) { return; }

    // Calculate the local transform: registry transform * user override
    glm::mat4 localTransform = regNode->transform * runtimeNode->transformOverride;

    // Calculate global transform: parent global * local transform
    runtimeNode->globalTransform = parentGlobalTransform * localTransform;

    // Mark this node as clean (optional since we're not checking dirty flags anymore)
    runtimeNode->isDirty = false;

    // Recursively update all children
    for (uint32_t childIdx : runtimeNode->childrenIdxs) {
        updateGlobalTransforms(childIdx, runtimeNode->globalTransform);
    }
}

// TinyNodeRT3D method implementation
void TinyNodeRT3D::addChild(uint32_t childIndex, std::vector<std::unique_ptr<TinyNodeRT3D>>& allrtNodes) {
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
    // Get the root node (index 0)
    TinyNodeRT3D* node0 = rtNodes[1].get();
    TinyNodeRT3D* node1 = rtNodes[10].get();

    // Calculate rotation: 90 degrees per second = π/2 radians per second
    float rotationSpeed = glm::radians(90.0f); // 90 degrees per second in radians
    float rotationThisFrame = rotationSpeed * dTime;
    
    // Create rotation matrix around Y axis
    glm::mat4 rotMat0 = glm::rotate(glm::mat4(1.0f), rotationThisFrame, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 rotMat1 = glm::rotate(glm::mat4(1.0f), -rotationThisFrame, glm::vec3(0.0f, 1.0f, 0.0f));
    
    // Apply rotation to the transform override
    node0->transformOverride = rotMat0 * node0->transformOverride;
    node1->transformOverride = rotMat1 * node1->transformOverride;

    // Update the entire transform hierarchy every frame (no dirty flag checking)
    updateGlobalTransforms(0);
}

