#include "TinyEngine/TinyProject.hpp"

uint32_t TinyProject::addTemplateFromModel(const TinyModelNew& model) {
    std::vector<uint32_t> meshRegistryIndex; // Ensure correct mapping
    for (const auto& mesh : model.meshes) {
        TinyHandle handle = registry->addMesh(mesh);
        meshRegistryIndex.push_back(handle.index);
    }

    std::vector<uint32_t> texRegistryIndex;
    for (const auto& texture : model.textures) {
        TinyHandle handle = registry->addTexture(texture);
        texRegistryIndex.push_back(handle.index);
    }

    std::vector<uint32_t> matRegistryIndex;
    for (const auto& material : model.materials) {
        TinyRegistry::MaterialData correctMat;

        // Remap the material's texture indices
        
        uint32_t localAlbIndex = material.localAlbTexture;
        bool localAlbValid = localAlbIndex >= 0 && localAlbIndex < static_cast<int>(texRegistryIndex.size());
        correctMat.setAlbTexIndex(localAlbValid ? texRegistryIndex[localAlbIndex] : 0);

        uint32_t localNrmlIndex = material.localNrmlTexture;
        bool localNrmlValid = localNrmlIndex >= 0 && localNrmlIndex < static_cast<int>(texRegistryIndex.size());
        correctMat.setNrmlTexIndex(localNrmlValid ? texRegistryIndex[localNrmlIndex] : 0);

        TinyHandle handle = registry->addMaterial(correctMat);
        matRegistryIndex.push_back(handle.index);
    }

    std::vector<uint32_t> skelRegistryIndex;
    for (const auto& skeleton : model.skeletons) {
        TinyHandle handle = registry->addSkeleton(skeleton);
        skelRegistryIndex.push_back(handle.index);
    }

    std::vector<TinyHandle> nodeRegistryHandle;
    TinyHandle rootHandle;

    UnorderedMap<int, int> localToGlobalNodeIndex; // Left: local index, Right: global index

    for (int i = 0; i < static_cast<int>(model.nodes.size()); ++i) {
        // Just occupy the index
        TinyHandle handle = registry->addNode(TinyNode());
        localToGlobalNodeIndex[i] = handle.index;

        nodeRegistryHandle.push_back(handle);
    }

    if (!nodeRegistryHandle.empty()) {
        rootHandle = nodeRegistryHandle[0];
    }

    for (int i = 0; i < static_cast<int>(model.nodes.size()); ++i) {

        const TinyNode& localNode = model.nodes[i];
        TinyNode& regNode = *registry->getNodeData(nodeRegistryHandle[i]);
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

                trueMesh3D.meshIndex = meshRegistryIndex[mesh3D.meshIndex];

                for (int localMatIndex : mesh3D.submeshMats) {
                    bool valid = localMatIndex >= 0 && localMatIndex < static_cast<int>(matRegistryIndex.size());
                    trueMesh3D.submeshMats.push_back(valid ? matRegistryIndex[localMatIndex] : 0);
                }

                bool hasValidSkeleton = mesh3D.skeletonNodeIndex >= 0 && mesh3D.skeletonNodeIndex < static_cast<int>(model.nodes.size());
                trueMesh3D.skeletonNodeIndex = hasValidSkeleton ? localToGlobalNodeIndex[mesh3D.skeletonNodeIndex] : -1; // Point to node
                
                regNode.make(std::move(trueMesh3D));
                break;
            }

            case TinyNode::Type::Skeleton3D: {
                const TinyNode::Skeleton3D& skel3D = localNode.asSkeleton3D();
                TinyNode::Skeleton3D trueSkel3D;
                bool hasValidSkeleton = skel3D.skeletonRegIndex >= 0 && skel3D.skeletonRegIndex < static_cast<int>(skelRegistryIndex.size());
                trueSkel3D.skeletonRegIndex = hasValidSkeleton ? skelRegistryIndex[skel3D.skeletonRegIndex] : -1; // Point to skeleton registry

                regNode.make(std::move(trueSkel3D));
                break;
            }
        }
    }

    // Print every node in registry
    for (int i = 0; i < static_cast<int>(nodeRegistryHandle.size()); ++i) {
        TinyNode* regNode = registry->getNodeData(nodeRegistryHandle[i]);
        
        printf("Node %d: Name='%s', Type=%d, Parent=%d, Children=[ ",
            i,
            regNode->name.c_str(),
            static_cast<int>(regNode->type),
            regNode->parent
        );
        for (size_t c = 0; c < regNode->children.size(); ++c) {
            printf("%d ", regNode->children[c]);
        }
        printf("]\n");
    }

    // Push root handle
    templates.push_back(rootHandle);
    return static_cast<uint32_t>(templates.size() - 1);
}