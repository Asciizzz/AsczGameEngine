#include "TinyEngine/TinyProject.hpp"

using HType = TinyHandle::Type;
using NType = TinyNode3D::Type;

// A quick function for range validation
template<typename T>
bool isValidIndex(int index, const std::vector<T>& vec) {
    return index >= 0 && index < static_cast<int>(vec.size());
}


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
        TinyRegistry::MaterialData correctMat;

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
        TinyHandle handle = registry->addSkeleton(skeleton);
        glbSkeleRegHandle.push_back(handle);
    }

    std::vector<TinyHandle> glbNodeRegHandle;

    UnorderedMap<int, TinyHandle> localNodeIndexToGlobalNodeHandle; // Left: local index, Right: global index

    for (int i = 0; i < static_cast<int>(model.nodes.size()); ++i) {
        // Just occupy the index
        TinyHandle handle = registry->addNode(TinyNode3D());
        localNodeIndexToGlobalNodeHandle[i] = handle;

        glbNodeRegHandle.push_back(handle);
    }

    for (int i = 0; i < static_cast<int>(model.nodes.size()); ++i) {
        const TinyNode3D& localNode = model.nodes[i];
        TinyNode3D& regNode = *registry->getNodeData(glbNodeRegHandle[i]);
        regNode.scope = localNode.scope;
        regNode.name = localNode.name;

        // Remapping children and parent
        regNode.parent = localNodeIndexToGlobalNodeHandle[localNode.parent.index];
        regNode.children.clear();
        for (const TinyHandle& localChild : localNode.children) {
            regNode.children.push_back(localNodeIndexToGlobalNodeHandle[localChild.index]);
        }

        // Remapping node data
        switch(localNode.type) {
            case NType::Node: {
                // Since transform is now in base class, we can just copy it directly
                regNode.transform = localNode.transform;
                regNode.make(TinyNode3D::Node{});
                break;
            }

            case NType::MeshRender: {
                const TinyNode3D::MeshRender& mesh3D = localNode.as<TinyNode3D::MeshRender>();
                TinyNode3D::MeshRender trueMesh3D;
                // Transform is now in base class
                regNode.transform = localNode.transform;

                trueMesh3D.mesh = glbMeshRegHandle[mesh3D.mesh.index];

                for (TinyHandle localMat : mesh3D.submeshMats) {
                    bool valid = isValidIndex(localMat.index, glbMatRegHandle);
                    trueMesh3D.submeshMats.push_back(valid ? glbMatRegHandle[localMat.index] : TinyHandle::invalid());
                }

                bool hasValidSkeleton = isValidIndex(mesh3D.skeleNode.index, glbNodeRegHandle);
                trueMesh3D.skeleNode = hasValidSkeleton ? localNodeIndexToGlobalNodeHandle[mesh3D.skeleNode.index] : TinyHandle::invalid();

                regNode.make(std::move(trueMesh3D));
                break;
            }

            case NType::Skeleton: {
                const TinyNode3D::Skeleton& skel3D = localNode.as<TinyNode3D::Skeleton>();
                TinyNode3D::Skeleton trueSkel3D;
                // Transform is now in base class
                regNode.transform = localNode.transform;
                
                bool hasValidSkeleton = isValidIndex(skel3D.skeleRegistry.index, glbSkeleRegHandle);
                // trueSkel3D.skeleRegistry = hasValidSkeleton ? glbSkeleRegHandle[skel3D.skeleRegistry.index] : TinyHandle::invalid();
                trueSkel3D.skeleRegistry = glbSkeleRegHandle[skel3D.skeleRegistry.index];

                regNode.make(std::move(trueSkel3D));
                break;
            }
        }
    }

    templates.push_back(TinyTemplate(glbNodeRegHandle));
    return static_cast<uint32_t>(templates.size() - 1);
}



// Recursive function to construct runtime node tree

void TinyProject::addNodeInstance(uint32_t templateIndex, uint32_t inheritIndex) {
    if (!isValidIndex(templateIndex, templates)) {
        printf("Error: Invalid template index %d\n", templateIndex);
        return;
    }

    UnorderedMap<TinyHandle, TinyHandle> registryToRuntimeNodeMap;

    const TinyTemplate& temp = templates[templateIndex];

    // First pass: create and append
    for (const TinyHandle& regHandle : temp.registryNodes) {
        TinyNode3D* regNode = registry->getNodeData(regHandle);
        if (!regNode) {
            printf("Warning: Registry node handle %llu is invalid, skipping.\n", regHandle.value);
            continue;
        }

        auto runtimeNode = MakeUnique<TinyNodeRuntime>();
        runtimeNode->regHandle = regHandle; // reference to the registry node

        // Runtime node cannot own the registry node
        registryToRuntimeNodeMap[regHandle] = TinyHandle(runtimeNodes.size(), HType::Node, false);
        runtimeNodes.push_back(std::move(runtimeNode));
    }

    // Second pass: set up the node (parent - child relationships, data, overrides)

    TinyHandle rootRuntimeHandle = inheritIndex < runtimeNodes.size() ?
        TinyHandle(inheritIndex, HType::Node, false) :
        TinyHandle(0, HType::Node, false); // Default to root if invalid

    for (uint32_t i = 0; i < static_cast<uint32_t>(temp.registryNodes.size()); ++i) {
        const TinyHandle& regHandle = temp.registryNodes[i];
        TinyNode3D* regNode = registry->getNodeData(regHandle);

        TinyHandle runtimeNodeHandle = registryToRuntimeNodeMap[regHandle];

        UniquePtr<TinyNodeRuntime>& runtimeNode = runtimeNodes[runtimeNodeHandle.index];

        // Remap children and parent (do not affect the children OR the parent, only apply to this current node)
        TinyHandle parentRegHandle = regNode->parent;

        bool hasParent = parentRegHandle.isValid() && registryToRuntimeNodeMap.count(parentRegHandle);

        // Default to root if not found
        TinyHandle parentRuntimeHandle = hasParent ? registryToRuntimeNodeMap[parentRegHandle] : rootRuntimeHandle;

        // If no parent, add child to root
        if (!hasParent) {
            runtimeNodes[inheritIndex]->children.push_back(runtimeNodeHandle);
        }

        runtimeNode->parent = parentRuntimeHandle;

        for (const TinyHandle& childRegHandle : regNode->children) {
            if (registryToRuntimeNodeMap.count(childRegHandle)) {
                TinyHandle childRuntimeHandle = registryToRuntimeNodeMap[childRegHandle];
                runtimeNode->children.push_back(childRuntimeHandle);
            }
        }

        // Construct the runtime node with override datas
        switch (regNode->type) {
            case NType::Node: {
                // Transform is now directly in the base class
                TinyNodeRuntime::Node_runtime nodeRuntime;
                nodeRuntime.transformOverride = regNode->transform;

                runtimeNode->make(nodeRuntime); // Automatic resolve, don't worry
                break;
            }

            case NType::MeshRender: {
                const auto& mesh3D = regNode->as<TinyNode3D::MeshRender>();

                TinyNodeRuntime::Mesh_runtime meshRuntime;
                meshRuntime.transformOverride = regNode->transform;

                // Get the true skeleton node in the runtime
                bool hasValidSkeleNode = mesh3D.skeleNode.isValid() && registryToRuntimeNodeMap.count(mesh3D.skeleNode);
                meshRuntime.skeleNodeOverride = hasValidSkeleNode ? registryToRuntimeNodeMap[mesh3D.skeleNode] : TinyHandle::invalid();

                runtimeNode->make(meshRuntime);
                break;
            }

            case NType::Skeleton: {
                const auto& skel3D = regNode->as<TinyNode3D::Skeleton>();

                const auto* skeleData = registry->getSkeletonData(skel3D.skeleRegistry);

                TinyNodeRuntime::Skeleton_runtime skelRuntime;
                skelRuntime.transformOverride = regNode->transform;
                skelRuntime.skeletonHandle = skel3D.skeleRegistry;
                skelRuntime.boneTransformsFinal.resize(
                    skeleData->names.size(), glm::mat4(1.0f)
                );

                runtimeNode->make(skelRuntime);
                break;
            }
        }
    }
}

void TinyProject::printRuntimeNodeRecursive(
    const UniquePtrVec<TinyNodeRuntime>& runtimeNodes,
    TinyRegistry* registry,
    const TinyHandle& runtimeHandle,
    int depth
) {
    const TinyNodeRuntime* rtNode = runtimeNodes[runtimeHandle.index].get();
    const TinyNode3D* regNode = registry->getNodeData(rtNode->regHandle);

    // Format runtime index padded to 3 chars
    char idxBuf[8];
    snprintf(idxBuf, sizeof(idxBuf), "%3u", runtimeHandle.index);

    // Indentation
    std::string indent(depth * 2, ' ');

    // Print: [idx] indent Name (Reg: regIndex, Parent: parentIndex)
    printf("[%s] %s%s (Reg: %u, Parent: %u)\n",
        idxBuf,
        indent.c_str(),
        regNode ? regNode->name.c_str() : "<invalid>",
        rtNode->regHandle.index,
        rtNode->parent.index);   // <-- Added here

    // Recurse for children
    for (const TinyHandle& child : rtNode->children) {
        printRuntimeNodeRecursive(runtimeNodes, registry, child, depth + 1);
    }
}

void TinyProject::printRuntimeNodeOrdered() {
    for (size_t i = 0; i < runtimeNodes.size(); ++i) {
        const UniquePtr<TinyNodeRuntime>& runtimeNode = runtimeNodes[i];
        if (!runtimeNode) continue;

        // Lookup registry node name
        std::string regName = "???";
        if (TinyNode3D* regNode = registry->getNodeData(runtimeNode->regHandle)) {
            regName = regNode->name;
        }

        // Parent index
        uint32_t parentIndex = runtimeNode->parent.isValid() 
            ? runtimeNode->parent.index 
            : std::numeric_limits<uint32_t>::max();

        // Print in ordered flat list
        printf("[%3zu] %s (Reg: %u, Parent: %u)\n",
            i,
            regName.c_str(),
            runtimeNode->regHandle.index,
            parentIndex);
    }
}

