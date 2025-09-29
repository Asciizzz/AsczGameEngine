#include "TinyEngine/TinyProject.hpp"


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

    std::vector<uint32_t> glbSkeletonRegIndex;
    for (const auto& skeleton : model.skeletons) {
        TinyHandle handle = registry->addSkeleton(skeleton);
        glbSkeletonRegIndex.push_back(handle.index);
    }

    std::vector<TinyHandle> glbNodeRegHandle;

    UnorderedMap<int, TinyHandle> localNodeIndexToGlobalNodeHandle; // Left: local index, Right: global index

    for (int i = 0; i < static_cast<int>(model.nodes.size()); ++i) {
        // Just occupy the index
        TinyHandle handle = registry->addNode(TinyNode());
        localNodeIndexToGlobalNodeHandle[i] = handle;

        glbNodeRegHandle.push_back(handle);
    }

    TinyHandle rootHandle;
    if (!glbNodeRegHandle.empty()) {
        rootHandle = glbNodeRegHandle[0];
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

        printf("Mapping local node %d to global node %d\n", i, glbNodeRegHandle[i].index);

        // Remapping node data
        switch(localNode.type) {
            case TinyNode::Type::Node3D: {
                const TinyNode::Node3D& node3D = localNode.asNode3D();

                regNode.make(node3D);
                break;
            }

            case TinyNode::Type::Mesh3D: {
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
                bool hasValidSkeleton = isValidIndex(skel3D.skeleRegistry.index, glbSkeletonRegIndex);
                trueSkel3D.skeleRegistry.index = hasValidSkeleton ? glbSkeletonRegIndex[skel3D.skeleRegistry.index] : -1; // Point to skeleton registry

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

    // Push root handle
    templates.push_back(rootHandle);
    return static_cast<uint32_t>(templates.size() - 1);
}