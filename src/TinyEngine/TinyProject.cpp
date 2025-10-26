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
    tinyFS->setTypeExt<TinyScene>     ("ascn", false, 0, 0.4f, 1.0f, 0.4f);
    tinyFS->setTypeExt<TinyTexture>   ("atex", false, 0, 0.4f, 0.4f, 1.0f);
    tinyFS->setTypeExt<TinyRMaterial> ("amat", true,  0, 1.0f, 0.4f, 1.0f);
    tinyFS->setTypeExt<TinyMesh>      ("amsh", false, 0, 1.0f, 1.0f, 0.4f);
    tinyFS->setTypeExt<TinySkeleton>  ("askl", true,  0, 0.4f, 1.0f, 1.0f);

    vkCreateSceneResources();

    // Create Main Scene (the active scene with a single root node)
    TinyScene mainScene("Main Scene");
    mainScene.addRoot("Root");
    mainScene.setSceneReq(sceneReq());

    // Create "Main Scene" as a non-deletable file in root directory
    TinyFS::Node::CFG sceneConfig;
    sceneConfig.deletable = false; // Make it non-deletable

    TinyHandle mainSceneFileHandle = tinyFS->addFile(tinyFS->rootHandle(), "Main Scene", std::move(mainScene), sceneConfig);
    TypeHandle mainSceneTypeHandle = tinyFS->fTypeHandle(mainSceneFileHandle);
    initialSceneHandle = mainSceneTypeHandle.handle; // Store the initial scene handle

    // Create camera and global UBO manager
    tinyCamera = MakeUnique<TinyCamera>(glm::vec3(0.0f, 0.0f, 0.0f), 45.0f, 0.1f, 1000.0f);
    tinyGlobal = MakeUnique<TinyGlobal>(2);
    tinyGlobal->createVkResources(deviceVK);

    // Create default material and texture
    TinyTexture defaultTexture = TinyTexture::createDefaultTexture();
    defaultTexture.vkCreate(deviceVK);

    defaultTextureHandle = tinyFS->rAdd(std::move(defaultTexture)).handle;

    TinyRMaterial defaultMaterial;
    defaultMaterial.setAlbTexIndex(0);
    defaultMaterial.setNrmlTexIndex(0);

    defaultMaterialHandle = tinyFS->rAdd(std::move(defaultMaterial)).handle;
}

TinyProject::~TinyProject() {
    // Cleanup everything first before destroying pools
    tinyGlobal.reset();
    tinyCamera.reset();
    tinyFS.reset();
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

        TinyHandle fnHandle = tinyFS->addFile(fnTexFolder, texture.name, std::move(texture));
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

        TinyHandle fnHandle = tinyFS->addFile(fnMeshFolder, mesh.name, std::move(mesh));
        TypeHandle tHandle = tinyFS->fTypeHandle(fnHandle);

        glbMeshRHandle.push_back(tHandle.handle);
    }

    // Import skeletons to registry
    std::vector<TinyHandle> glbSkeleRHandle;
    for (auto& skeleton : model.skeletons) {
        TinyHandle fnHandle = tinyFS->addFile(fnSkeleFolder, skeleton.name, std::move(skeleton));
        TypeHandle tHandle = tinyFS->fTypeHandle(fnHandle);

        glbSkeleRHandle.push_back(tHandle.handle);
    }

    // If scene has no nodes, return early
    if (model.nodes.empty()) return fnModelFolder;

    // Create scene with nodes - preserve hierarchy but remap resource references
    TinyScene scene(model.name);
    scene.setSceneReq(sceneReq());

    // First pass: Insert empty nodes and store their handles
    std::vector<TinyHandle> nodeHandles;

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
        };

        // Remap children handles
        for (const TinyHandle& childHandle : originalNode.childrenHandles) {
            if (validIndex(childHandle, nodeHandles)) {
                childrenHandles.push_back(nodeHandles[childHandle.index]);
            }
        }

        scene.setNodeParent(nodeHandle, parentHandle);
        scene.setNodeChildren(nodeHandle, childrenHandles);

        // Add component with scene API to ensure proper handling
        if (originalNode.has<TinyNode::T3D>()) {
            const auto* ogTransform = originalNode.get<TinyNode::T3D>();
            auto* newTransform = scene.writeComp<TinyNode::T3D>(nodeHandle);
            *newTransform = *ogTransform;
        }

        if (originalNode.has<TinyNode::MR3D>()) {
            const auto* ogMeshRender = originalNode.get<TinyNode::MR3D>();
            auto* newMeshRender = scene.writeComp<TinyNode::MR3D>(nodeHandle);

            if (validIndex(ogMeshRender->pMeshHandle, glbMeshRHandle)) {
                newMeshRender->pMeshHandle = glbMeshRHandle[ogMeshRender->pMeshHandle.index];
            }

            if (validIndex(ogMeshRender->skeleNodeHandle, nodeHandles)) {
                newMeshRender->skeleNodeHandle = nodeHandles[ogMeshRender->skeleNodeHandle.index];
            }
        }

        if (originalNode.has<TinyNode::BA3D>()) {
            const auto* ogBoneAttach = originalNode.get<TinyNode::BA3D>();
            auto* newBoneAttach = scene.writeComp<TinyNode::BA3D>(nodeHandle);

            if (validIndex(ogBoneAttach->skeleNodeHandle, nodeHandles)) {
                newBoneAttach->skeleNodeHandle = nodeHandles[ogBoneAttach->skeleNodeHandle.index];
            }

            newBoneAttach->boneIndex = ogBoneAttach->boneIndex;
        }

        if (originalNode.has<TinyNode::SK3D>()) {
            const auto* ogSkeleComp = originalNode.get<TinyNode::SK3D>();
            auto* newSkeleRT = scene.writeComp<TinyNode::SK3D>(nodeHandle);

            if (validIndex(ogSkeleComp->pSkeleHandle, glbSkeleRHandle)) {
                // Construct new skeleton runtime from the original skeleton
                newSkeleRT->set(glbSkeleRHandle[ogSkeleComp->pSkeleHandle.index]);
            }
        }

        if (originalNode.has<TinyNode::AN3D>()) {
            const auto* ogAnimeComp = originalNode.get<TinyNode::AN3D>();
            auto* newAnimeComp = scene.writeComp<TinyNode::AN3D>(nodeHandle);

            // Very complex: remapping of every animation channel's node
            *newAnimeComp = model.animations[ogAnimeComp->pAnimeHandle.index];

            for (auto& channel : newAnimeComp->channels) {
                if (validIndex(channel.node, nodeHandles)) {
                    channel.node = nodeHandles[channel.node.index];
                }
            }
        }
    }

    // Add scene to registry
    TinyHandle fnHandle = tinyFS->addFile(fnModelFolder, scene.name, &scene);
    TypeHandle tHandle = tinyFS->fTypeHandle(fnHandle);

    // Return the model folder handle instead of the scene handle
    return fnModelFolder;
}



void TinyProject::addSceneInstance(TinyHandle fromHandle, TinyHandle toHandle, TinyHandle parentHandle) {
    const TinyScene* fromScene = fs().rGet<TinyScene>(fromHandle);
    TinyScene* toScene = fs().rGet<TinyScene>(toHandle);
    if (toHandle == fromHandle || !toScene || !fromScene) return;

    toScene->addScene(fromScene, parentHandle);
}


void TinyProject::vkCreateSceneResources() {
    skinDescLayout.create(*deviceVK, {
        {0, DescType::StorageBuffer, 1, ShaderStage::Vertex, nullptr}
    });

    skinDescPool.create(*deviceVK, {
        {DescType::StorageBuffer, maxSkeletons}
    }, maxSkeletons);

    // Create dummy skin descriptor set for rigged meshes without skeleton
    createDummySkinDescriptorSet();

    sharedReq.fsRegistry = &fs().registry();
    sharedReq.deviceVK = deviceVK;
    sharedReq.skinDescPool = skinDescPool;
    sharedReq.skinDescLayout = skinDescLayout;
}

void TinyProject::createDummySkinDescriptorSet() {
    // Create dummy descriptor set (allocate from same pool as real skeletons)
    dummySkinDescSet.allocate(deviceVK->device, skinDescPool.get(), skinDescLayout.get());

    // Create dummy skin buffer with 1 identity matrix
    glm::mat4 identityMatrix = glm::mat4(1.0f);
    VkDeviceSize bufferSize = sizeof(glm::mat4);

    dummySkinBuffer
        .setDataSize(bufferSize)
        .setUsageFlags(BufferUsage::Storage)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(deviceVK)
        .mapAndCopy(&identityMatrix);

    // Update descriptor set with dummy buffer info
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = dummySkinBuffer.get();
    bufferInfo.offset = 0;
    bufferInfo.range = bufferSize;

    DescWrite()
        .setDstSet(dummySkinDescSet)
        .setType(DescType::StorageBuffer)
        .setDescCount(1)
        .setBufferInfo({ bufferInfo })
        .updateDescSets(deviceVK->device);
}