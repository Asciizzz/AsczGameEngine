#include "TinyEngine/TinyProject.hpp"
#include "TinyEngine/TinyLoader.hpp"

using NTypes = TinyNode::Types;

using namespace TinyVK;

// A quick function for range validation
template<typename T>
bool validIndex(TinyHandle handle, const std::vector<T>& vec) {
    return handle.valid() && handle.index < vec.size();
}

TinyProject::TinyProject(const TinyVK::Device* deviceVK) : deviceVK(deviceVK) {
    tinyFS = MakeUnique<TinyFS>();

    // ext - safeDelete - priority - r - g - b
    tinyFS->setTypeExt<TinyScene>     ("ascn", false, 0, 0.8f, 1.0f, 0.8f);
    tinyFS->setTypeExt<TinyTexture>   ("atex", false, 0, 0.8f, 0.8f, 1.0f);
    tinyFS->setTypeExt<TinyRMaterial> ("amat", true,  0, 1.0f, 0.8f, 1.0f);
    tinyFS->setTypeExt<TinyMesh>      ("amsh", false, 0, 1.0f, 1.0f, 0.8f);
    tinyFS->setTypeExt<TinySkeleton>  ("askl", true,  0, 1.0f, 0.6f, 0.4f);
    tinyFS->setTypeExt<TinySkeletonRT>("rskl", false, 0, 1.0f, 0.4f, 0.2f);
    tinyFS->setTypeExt<TinyAnimation> ("anim", false, 0, 0.8f, 1.0f, 0.6f);

    vkCreateSceneResources();

    // Create Main Scene (the active scene with a single root node)
    TinyScene mainScene;
    mainScene.name = "Main Scene";
    mainScene.addRoot("Root");
    mainScene.setSceneReq(sceneReq());

    // Create "Main Scene" as a non-deletable file in root directory
    TinyFS::Node::CFG sceneConfig;
    sceneConfig.deletable = false; // Make it non-deletable

    TinyHandle mainSceneFileHandle = tinyFS->addFile(tinyFS->rootHandle(), "Main Scene", std::move(&mainScene), sceneConfig);
    TypeHandle mainSceneTypeHandle = tinyFS->fTypeHandle(mainSceneFileHandle);
    initialSceneHandle = mainSceneTypeHandle.handle; // Store the initial scene handle

    // Create camera and global UBO manager
    tinyCamera = MakeUnique<TinyCamera>(glm::vec3(0.0f, 0.0f, 0.0f), 45.0f, 0.1f, 1000.0f);
    tinyGlobal = MakeUnique<TinyGlobal>(2);
    tinyGlobal->createVkResources(deviceVK);

    // Create default material and texture
    TinyTexture defaultTexture = TinyTexture::createDefaultTexture();
    defaultTexture.vkCreate(deviceVK);

    defaultTextureHandle = tinyFS->rAdd(defaultTexture).handle;

    TinyRMaterial defaultMaterial;
    defaultMaterial.setAlbTexIndex(0);
    defaultMaterial.setNrmlTexIndex(0);

    defaultMaterialHandle = tinyFS->rAdd(defaultMaterial).handle;
}

TinyHandle TinyProject::addModel(TinyModel& model, TinyHandle parentFolder) {
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

        TinyHandle fnHandle = tinyFS->addFile(fnTexFolder, texture.name, std::move(&texture));
        TypeHandle tHandle = tinyFS->fTypeHandle(fnHandle);

        glbTexRHandle.push_back(tHandle.handle);
    }

    // Import materials to registry with remapped texture references
    std::vector<TinyHandle> glmMatRHandle;
    for (const auto& material : model.materials) {
        TinyRMaterial correctMat;
        correctMat.name = material.name;

        // Remap the material's texture indices
        uint32_t localAlbIndex = material.localAlbTexture;
        bool localAlbValid = localAlbIndex >= 0 && localAlbIndex < static_cast<int>(glbTexRHandle.size());
        correctMat.setAlbTexIndex(localAlbValid ? glbTexRHandle[localAlbIndex].index : 0);

        uint32_t localNrmlIndex = material.localNrmlTexture;
        bool localNrmlValid = localNrmlIndex >= 0 && localNrmlIndex < static_cast<int>(glbTexRHandle.size());
        correctMat.setNrmlTexIndex(localNrmlValid ? glbTexRHandle[localNrmlIndex].index : 0);

        TinyHandle fnHandle = tinyFS->addFile(fnMatFolder, correctMat.name, &correctMat);
        TypeHandle tHandle = tinyFS->fTypeHandle(fnHandle);

        glmMatRHandle.push_back(tHandle.handle);
    }

    // Import meshes to registry with remapped material references
    std::vector<TinyHandle> glbMeshRHandle;
    for (auto& mesh : model.meshes) {
        // Remap submeshes' material indices
        std::vector<TinySubmesh> remappedSubmeshes = mesh.submeshes;
        for (auto& submesh : remappedSubmeshes) {
            bool valid = validIndex(submesh.material, glmMatRHandle);
            submesh.material = valid ? glmMatRHandle[submesh.material.index] : TinyHandle();
        }

        mesh.vkCreate(deviceVK);

        mesh.setSubmeshes(remappedSubmeshes);

        TinyHandle fnHandle = tinyFS->addFile(fnMeshFolder, mesh.name, std::move(&mesh));
        TypeHandle tHandle = tinyFS->fTypeHandle(fnHandle);

        glbMeshRHandle.push_back(tHandle.handle);
    }

    // Import skeletons to registry
    std::vector<TinyHandle> glbSkeleRHandle;
    for (auto& skeleton : model.skeletons) {
        TinyHandle fnHandle = tinyFS->addFile(fnSkeleFolder, skeleton.name, std::move(&skeleton));
        TypeHandle tHandle = tinyFS->fTypeHandle(fnHandle);

        glbSkeleRHandle.push_back(tHandle.handle);
    }

    // Create scene with nodes - preserve hierarchy but remap resource references
    TinyScene scene(model.name);
    scene.setSceneReq(sceneReq());

    // First pass: Insert empty nodes and store their handles
    std::vector<TinyHandle> nodeHandles;
    nodeHandles.reserve(model.nodes.size());

    for (const auto& node : model.nodes) {
        nodeHandles.push_back(scene.addNodeRaw(node.name));
    }

    // Set the root node to the first node's actual handle
    // This in theory should be correct if you load the proper model
    if (!nodeHandles.empty()) scene.setRoot(nodeHandles[0]);

    // Second pass: Remap parent/child relationships and add components
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        TinyHandle nodeHandle = nodeHandles[i];

        TinyHandle parentHandle;
        std::vector<TinyHandle> childrenHandles;

        const TinyNode& originalNode = model.nodes[i];

        // Remap parent handle
        if (validIndex(originalNode.parentHandle, nodeHandles)) {
            parentHandle = nodeHandles[originalNode.parentHandle.index];
        }

        // Remap children handles
        for (const TinyHandle& childHandle : originalNode.childrenHandles) {
            if (validIndex(childHandle, nodeHandles)) {
                childrenHandles.push_back(nodeHandles[childHandle.index]);
            }
        }

        scene.setNodeParent(nodeHandle, parentHandle);
        scene.setNodeChildren(nodeHandle, childrenHandles);

        // Add component with scene API to ensure proper handling
        if (originalNode.has<TinyNode::Node3D>()) { // Should have, otherwise wtf are you doing
            const TinyNode::Node3D* ogTransform = originalNode.get<TinyNode::Node3D>();
            TinyNode::Node3D newTransform = *ogTransform;

            scene.nodeAddComp<TinyNode::Node3D>(nodeHandle, newTransform);
        }

        if (originalNode.has<TinyNode::MeshRender>()) {
            const TinyNode::MeshRender* ogMeshComp = originalNode.get<TinyNode::MeshRender>();

            TinyHandle newMeshHandle;
            if (validIndex(ogMeshComp->meshHandle, glbMeshRHandle)) {
                newMeshHandle = glbMeshRHandle[ogMeshComp->meshHandle.index];
            }

            TinyHandle newSkeleNodeHandle;
            if (validIndex(ogMeshComp->skeleNodeHandle, nodeHandles)) {
                newSkeleNodeHandle = nodeHandles[ogMeshComp->skeleNodeHandle.index];
            }

            TinyNode::MeshRender newMeshComp;
            newMeshComp.meshHandle = newMeshHandle;
            newMeshComp.skeleNodeHandle = newSkeleNodeHandle;

            scene.nodeAddComp<TinyNode::MeshRender>(nodeHandle, newMeshComp);
        }

        if (originalNode.has<TinyNode::Skeleton>()) {
            const TinyNode::Skeleton* ogSkelComp = originalNode.get<TinyNode::Skeleton>();

            TinyHandle newSkeleHandle;
            if (validIndex(ogSkelComp->skeleHandle, glbSkeleRHandle)) {
                newSkeleHandle = glbSkeleRHandle[ogSkelComp->skeleHandle.index];
            }

            TinyNode::Skeleton remappedSkelComp;
            remappedSkelComp.skeleHandle = newSkeleHandle;

            scene.nodeAddComp<TinyNode::Skeleton>(nodeHandle, remappedSkelComp);
        }
    }

    // Add scene to registry
    TinyHandle fnHandle = tinyFS->addFile(fnModelFolder, scene.name, &scene);
    TypeHandle tHandle = tinyFS->fTypeHandle(fnHandle);

    // Return the model folder handle instead of the scene handle
    return fnModelFolder;
}



void TinyProject::addSceneInstance(TinyHandle fromHandle, TinyHandle toHandle, TinyHandle parentHandle) {
    TinyScene* targetScene = registryRef().get<TinyScene>(toHandle);
    if (!targetScene) return;

    // Use root node if no valid parent provided
    // Use the scene's addScene method to merge the source scene into target scene
    targetScene->addScene(fromHandle, parentHandle);
    
    // Update transforms for the entire target scene
    targetScene->update();
}


void TinyProject::vkCreateSceneResources() {
    skinDescLayout.create(*deviceVK, {
        {0, DescType::StorageBuffer, 1, ShaderStage::Vertex, nullptr}
    });

    skinDescPool.create(*deviceVK, {
        {DescType::StorageBuffer, 1}
    }, maxSkeletons);

    sharedReq.fsRegistry = &registryRef();
    sharedReq.device = deviceVK;
    sharedReq.skinDescPool = skinDescPool;
    sharedReq.skinDescLayout = skinDescLayout;
}