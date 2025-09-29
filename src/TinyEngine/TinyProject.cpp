#include "TinyEngine/TinyProject.hpp"

uint32_t TinyProject::addTemplateFromModel(const TinyModelNew& model) {
    std::vector<uint32_t> glbMeshRegIndex; // Ensure correct mapping
    for (const auto& mesh : model.meshes) {
        TinyHandle handle = registry->addMesh(mesh);
        glbMeshRegIndex.push_back(handle.index);
    }

    std::vector<uint32_t> glbTexRegIndex;
    for (const auto& texture : model.textures) {
        TinyHandle handle = registry->addTexture(texture);
        glbTexRegIndex.push_back(handle.index);
    }

    std::vector<uint32_t> glbMatRegIndex;
    for (const auto& material : model.materials) {
        TinyRegistry::MaterialData correctMat;

        // Remap the material's texture indices
        
        uint32_t localAlbIndex = material.localAlbTexture;
        bool localAlbValid = localAlbIndex >= 0 && localAlbIndex < static_cast<int>(glbTexRegIndex.size());
        correctMat.setAlbTexIndex(localAlbValid ? glbTexRegIndex[localAlbIndex] : 0);

        uint32_t localNrmlIndex = material.localNrmlTexture;
        bool localNrmlValid = localNrmlIndex >= 0 && localNrmlIndex < static_cast<int>(glbTexRegIndex.size());
        correctMat.setNrmlTexIndex(localNrmlValid ? glbTexRegIndex[localNrmlIndex] : 0);

        TinyHandle handle = registry->addMaterial(correctMat);
        glbMatRegIndex.push_back(handle.index);
    }

    std::vector<uint32_t> glbSkeletonRegIndex;
    for (const auto& skeleton : model.skeletons) {
        TinyHandle handle = registry->addSkeleton(skeleton);
        glbSkeletonRegIndex.push_back(handle.index);
    }

    std::vector<TinyHandle> glbNodeRegHandle;

    UnorderedMap<int, int> localToGlobalNodeIndex; // Left: local index, Right: global index

    for (int i = 0; i < static_cast<int>(model.nodes.size()); ++i) {
        // Just occupy the index
        TinyHandle handle = registry->addNode(TinyNode());
        localToGlobalNodeIndex[i] = handle.index;

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
        regNode.parent = localToGlobalNodeIndex[localNode.parent];
        regNode.children.clear();
        for (int localChild : localNode.children) {
            regNode.children.push_back(localToGlobalNodeIndex[localChild]);
        }

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

                trueMesh3D.meshIndex = glbMeshRegIndex[mesh3D.meshIndex];

                for (int localMatIndex : mesh3D.submeshMats) {
                    bool valid = localMatIndex >= 0 && localMatIndex < static_cast<int>(glbMatRegIndex.size());
                    trueMesh3D.submeshMats.push_back(valid ? glbMatRegIndex[localMatIndex] : 0);
                }

                bool hasValidSkeleton = mesh3D.skeletonNodeIndex >= 0 && mesh3D.skeletonNodeIndex < static_cast<int>(model.nodes.size());
                trueMesh3D.skeletonNodeIndex = hasValidSkeleton ? localToGlobalNodeIndex[mesh3D.skeletonNodeIndex] : -1; // Point to node
                
                regNode.make(std::move(trueMesh3D));
                break;
            }

            case TinyNode::Type::Skeleton3D: {
                const TinyNode::Skeleton3D& skel3D = localNode.asSkeleton3D();
                TinyNode::Skeleton3D trueSkel3D;
                bool hasValidSkeleton = skel3D.skeletonRegIndex >= 0 && skel3D.skeletonRegIndex < static_cast<int>(glbSkeletonRegIndex.size());
                trueSkel3D.skeletonRegIndex = hasValidSkeleton ? glbSkeletonRegIndex[skel3D.skeletonRegIndex] : -1; // Point to skeleton registry

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
            regNode->parent
        );
        for (size_t c = 0; c < regNode->children.size(); ++c) {
            printf("%d ", regNode->children[c]);
        }
        printf("]");
        // Reset color
        printf("\033[0m\n");

        // Print additional info
        if (regNode->isMesh3D()) {
            const auto& mesh = regNode->asMesh3D();
            printf(" -> MeshID=%d, MaterialIDs=[ ", mesh.meshIndex);
            for (size_t m = 0; m < mesh.submeshMats.size(); ++m) {
                printf("%d ", mesh.submeshMats[m]);
            }
            printf("], SkeNodeID=%d\n", mesh.skeletonNodeIndex);
        } else if (regNode->isSkeleton3D()) {
            const auto& skel = regNode->asSkeleton3D();
            printf(" -> SkeRegID=%d\n", skel.skeletonRegIndex);
        }
    }

    // Push root handle
    templates.push_back(rootHandle);
    return static_cast<uint32_t>(templates.size() - 1);
}