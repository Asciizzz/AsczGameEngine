#include "TinyEngine/TinyProject.hpp"

using HType = TinyHandle::Type;
using NType = TinyNode::Type;

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
        TinyHandle handle = registry->addNode(TinyNode());
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

        // Remapping node data
        switch(localNode.type) {
            case NType::Node3D: {
                const TinyNode::Node3D& node3D = localNode.asNode3D();

                regNode.make(node3D);
                break;
            }

            case NType::Mesh3D: {
                const TinyNode::Mesh3D& mesh3D = localNode.asMesh3D();
                TinyNode::Mesh3D trueMesh3D;
                trueMesh3D.transform = mesh3D.transform;

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

            case TinyNode::Type::Skeleton3D: {
                const TinyNode::Skeleton3D& skel3D = localNode.asSkeleton3D();
                TinyNode::Skeleton3D trueSkel3D;
                bool hasValidSkeleton = isValidIndex(skel3D.skeleRegistry.index, glbSkeleRegHandle);
                // trueSkel3D.skeleRegistry = hasValidSkeleton ? glbSkeleRegHandle[skel3D.skeleRegistry.index] : TinyHandle::invalid();
                trueSkel3D.skeleRegistry = glbSkeleRegHandle[skel3D.skeleRegistry.index];

                regNode.make(std::move(trueSkel3D));
                break;
            }
        }
    }

    // Print every node in registry
    printf("\033[1;34m--- Node Registry Dump ---\033[0m\n");
    for (int i = 0; i < static_cast<int>(glbNodeRegHandle.size()); ++i) {
        TinyNode* regNode = registry->getNodeData(glbNodeRegHandle[i]);
        // Color green
        printf("\033[32mNode %d: Name='%s', Parent=%d, Children=[ ",
            glbNodeRegHandle[i].index,
            regNode->name.c_str(),
            regNode->parent.index
        );
        for (size_t c = 0; c < regNode->children.size(); ++c) {
            printf("%d ", regNode->children[c].index);
        }
        printf("]");
        // Reset color
        printf("\033[0m\n");

        // Print additional info
        if (regNode->isMesh3D()) {
            const auto& meshNode = regNode->asMesh3D();
            printf(" -> MeshID=%d, MaterialIDs=[ ", meshNode.mesh.index);
            for (size_t m = 0; m < meshNode.submeshMats.size(); ++m) {
                printf("%d ", meshNode.submeshMats[m].index);
            }
            printf("], SkeNodeID=%d\n", meshNode.skeleNode.index);
        } else if (regNode->isSkeleton3D()) {
            const auto& skeleNode = regNode->asSkeleton3D();
            printf(" -> SkeRegID=%d\n", skeleNode.skeleRegistry.index);
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

    // First pass: create and append all runtime nodes

    for (const TinyHandle& regHandle : temp.registryNodes) {
        TinyNode* regNode = registry->getNodeData(regHandle);
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

    for (uint32_t i = 0; i < static_cast<uint32_t>(temp.registryNodes.size()); ++i) {
        const TinyHandle& regHandle = temp.registryNodes[i];
        TinyNode* regNode = registry->getNodeData(regHandle);

        TinyHandle runtimeNodeHandle = registryToRuntimeNodeMap[regHandle];

        UniquePtr<TinyNodeRuntime>& runtimeNode = runtimeNodes[runtimeNodeHandle.index];

        // Remap children and parent (do not affect the children OR the parent, only apply to this current node)
        TinyHandle parentRegHandle = regNode->parent;

        bool registryExists = registryToRuntimeNodeMap.count(parentRegHandle);
        bool noParent = parentRegHandle == TinyHandle::invalid();

        TinyHandle parentRuntimeHandle = registryToRuntimeNodeMap.count(parentRegHandle) ?
            registryToRuntimeNodeMap[parentRegHandle] : TinyHandle(0, HType::Node, false); // Default to root if not found

        for (const TinyHandle& childRegHandle : regNode->children) {
            if (registryToRuntimeNodeMap.count(childRegHandle)) {
                TinyHandle childRuntimeHandle = registryToRuntimeNodeMap[childRegHandle];
                runtimeNode->children.push_back(childRuntimeHandle);
            }
        }

        // Construct the runtime node with override datas
        switch (regNode->type) {
            case NType::Node3D: {
                const TinyNode::Node3D& node3D = regNode->asNode3D();

                TinyNodeRuntime::Node3D_runtime node3DRuntime;
                node3DRuntime.transformOverride = node3D.transform;

                runtimeNode->make(node3DRuntime); // Automatic resolve, don't worry
                break;
            }

            case NType::Mesh3D: {
                const TinyNode::Mesh3D& mesh3D = regNode->asMesh3D();

                TinyNodeRuntime::Mesh3D_runtime mesh3DRuntime;
                mesh3DRuntime.transformOverride = mesh3D.transform;

                mesh3DRuntime.submeshTransformsOverride.resize(mesh3D.submeshMats.size(), glm::mat4(1.0f));
                mesh3DRuntime.submeshMatsOverride = mesh3D.submeshMats;
                mesh3DRuntime.skeletonNodeOverride = mesh3D.skeleNode;

                runtimeNode->make(mesh3DRuntime);
                break;
            }

            case NType::Skeleton3D: {
                const TinyNode::Skeleton3D& skel3D = regNode->asSkeleton3D();

                const TinySkeleton* skeleData = registry->getSkeletonData(skel3D.skeleRegistry);

                TinyNodeRuntime::Skeleton3D_runtime skel3DRuntime;
                skel3DRuntime.skeletonHandle = skel3D.skeleRegistry;
                skel3DRuntime.boneTransformsOverride.resize(
                    skeleData->names.size(), glm::mat4(1.0f)
                );

                runtimeNode->make(skel3DRuntime);
                break;
            }
        }
    }
}

void TinyProject::printRuntimeNodeRecursive(
    const std::vector<UniquePtr<TinyNodeRuntime>>& runtimeNodes,
    TinyRegistry* registry,
    const TinyHandle& runtimeHandle,
    int depth
) {
    const TinyNodeRuntime* rtNode = runtimeNodes[runtimeHandle.index].get();
    const TinyNode* regNode = registry->getNodeData(rtNode->regHandle);

    // Format runtime index padded to 3 chars
    char idxBuf[8];
    snprintf(idxBuf, sizeof(idxBuf), "%3u", runtimeHandle.index);

    // Indentation
    std::string indent(depth * 2, ' ');

    // Print line: [idx] indent Name (Reg: regIndex)
    printf("[%s] %s%s (Reg: %u)\n",
        idxBuf,
        indent.c_str(),
        regNode ? regNode->name.c_str() : "<invalid>",
        rtNode->regHandle.index);

    // Recurse for children
    for (const TinyHandle& child : rtNode->children) {
        printRuntimeNodeRecursive(runtimeNodes, registry, child, depth + 1);
    }
}