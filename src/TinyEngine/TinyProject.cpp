#include "tinyEngine/tinyProject.hpp"
#include "tinyEngine/tinyLoader.hpp"

using NTypes = tinyNodeRT::Types;

using namespace tinyVK;

// A quick function for range validation
template<typename T>
bool validIndex(tinyHandle handle, const std::vector<T>& vec) {
    return handle.valid() && handle.index < vec.size();
}

tinyProject::tinyProject(const tinyVK::Device* deviceVK) : deviceVK(deviceVK) {
    fs_ = MakeUnique<tinyFS>();

    // ext - safeDelete - priority - r - g - b
    fs_->setTypeExt<tinySceneRT>     ("ascn", false, 0, 0.4f, 1.0f, 0.4f);
    fs_->setTypeExt<tinyTexture>   ("atex", false, 0, 0.4f, 0.4f, 1.0f);
    fs_->setTypeExt<tinyRMaterial> ("amat", true,  0, 1.0f, 0.4f, 1.0f);
    fs_->setTypeExt<tinyMesh>      ("amsh", false, 0, 1.0f, 1.0f, 0.4f);
    fs_->setTypeExt<tinySkeleton>  ("askl", true,  0, 0.4f, 1.0f, 1.0f);

    vkCreateSceneResources();

    // Create Main Scene (the active scene with a single root node)
    tinySceneRT mainScene("Main Scene");
    mainScene.addRoot("Root");
    mainScene.setSceneReq(sceneReq());

    // Create "Main Scene" as a non-deletable file in root directory
    tinyFS::Node::CFG sceneConfig;
    sceneConfig.deletable = false; // Make it non-deletable

    tinyHandle mainSceneFileHandle = fs_->addFile(fs_->rootHandle(), "Main Scene", std::move(mainScene), sceneConfig);
    typeHandle mainScenetypeHandle = fs_->ftypeHandle(mainSceneFileHandle);
    initialSceneHandle = mainScenetypeHandle.handle; // Store the initial scene handle

    // Create camera and global UBO manager
    camera_ = MakeUnique<tinyCamera>(glm::vec3(0.0f, 0.0f, 0.0f), 45.0f, 0.1f, 1000.0f);
    global_ = MakeUnique<tinyGlobal>(2);
    global_->vkCreate(deviceVK);

    // Create default material and texture
    tinyTexture defaultTexture = tinyTexture::createDefaultTexture();
    defaultTexture.vkCreate(deviceVK);

    defaultTextureHandle = fs_->rAdd(std::move(defaultTexture)).handle;

    tinyRMaterial defaultMaterial;
    defaultMaterial.setAlbTexIndex(0);
    defaultMaterial.setNrmlTexIndex(0);

    defaultMaterialHandle = fs_->rAdd(std::move(defaultMaterial)).handle;
}

tinyProject::~tinyProject() {
    // Cleanup everything first before destroying pools
    global_.reset();
    camera_.reset();
    fs_.reset();
}



tinyHandle tinyProject::addModel(tinyModel& model, tinyHandle parentFolder) {
    // Use root folder if no valid parent provided
    if (!parentFolder.valid()) {
        parentFolder = fs_->rootHandle();
    }
    
    // Create a folder for the model in the specified parent
    tinyHandle fnModelFolder = fs_->addFolder(parentFolder, model.name);
    tinyHandle fnTexFolder = fs_->addFolder(fnModelFolder, "Textures");
    tinyHandle fnMatFolder = fs_->addFolder(fnModelFolder, "Materials");
    tinyHandle fnMeshFolder = fs_->addFolder(fnModelFolder, "Meshes");
    tinyHandle fnSkeleFolder = fs_->addFolder(fnModelFolder, "Skeletons");

    // Note: fnHandle - handle to file node in tinyFS's fnodes
    //       tHandle - handle to the actual data in the registry (infused with Type info for tinyFS usage)

    // Import textures to registry
    std::vector<tinyHandle> glbTexRHandle;
    for (auto& texture : model.textures) {
        texture.vkCreate(deviceVK);

        tinyHandle fnHandle = fs_->addFile(fnTexFolder, texture.name, std::move(texture));
        typeHandle tHandle = fs_->ftypeHandle(fnHandle);

        glbTexRHandle.push_back(tHandle.handle);
    }

    // Import materials to registry with remapped texture references
    std::vector<tinyHandle> glmMatRHandle;
    for (const auto& material : model.materials) {
        tinyRMaterial correctMat;
        correctMat.name = material.name;

        // Remap the material's texture indices
        uint32_t localAlbIndex = material.localAlbTexture;
        bool localAlbValid = localAlbIndex >= 0 && localAlbIndex < static_cast<int>(glbTexRHandle.size());
        correctMat.setAlbTexIndex(localAlbValid ? glbTexRHandle[localAlbIndex].index : 0);

        uint32_t localNrmlIndex = material.localNrmlTexture;
        bool localNrmlValid = localNrmlIndex >= 0 && localNrmlIndex < static_cast<int>(glbTexRHandle.size());
        correctMat.setNrmlTexIndex(localNrmlValid ? glbTexRHandle[localNrmlIndex].index : 0);

        tinyHandle fnHandle = fs_->addFile(fnMatFolder, correctMat.name, std::move(correctMat));
        typeHandle tHandle = fs_->ftypeHandle(fnHandle);

        glmMatRHandle.push_back(tHandle.handle);
    }

    // Import meshes to registry with remapped material references
    std::vector<tinyHandle> glbMeshRHandle;
    for (auto& mesh : model.meshes) {
        // Remap submeshes' material indices
        std::vector<tinySubmesh> remappedSubmeshes = mesh.submeshes();
        for (auto& submesh : remappedSubmeshes) {
            bool valid = validIndex(submesh.material, glmMatRHandle);
            submesh.material = valid ? glmMatRHandle[submesh.material.index] : tinyHandle();
        }

        mesh.vkCreate(deviceVK);

        mesh.setSubmeshes(remappedSubmeshes);

        tinyHandle fnHandle = fs_->addFile(fnMeshFolder, mesh.name, std::move(mesh));
        typeHandle tHandle = fs_->ftypeHandle(fnHandle);

        glbMeshRHandle.push_back(tHandle.handle);
    }

    // Import skeletons to registry
    std::vector<tinyHandle> glbSkeleRHandle;
    for (auto& skeleton : model.skeletons) {
        tinyHandle fnHandle = fs_->addFile(fnSkeleFolder, skeleton.name, std::move(skeleton));
        typeHandle tHandle = fs_->ftypeHandle(fnHandle);

        glbSkeleRHandle.push_back(tHandle.handle);
    }

    // If scene has no nodes, return early
    if (model.nodes.empty()) return fnModelFolder;

    // Create scene with nodes - preserve hierarchy but remap resource references
    tinySceneRT scene(model.name);
    scene.setSceneReq(sceneReq());

    // First pass: Insert empty nodes and store their handles
    // Note: Normally we would've used a unordered_map
    //       but I trust the model to be imported from the
    //       tinyLoader which guarantees index stability.
    std::vector<tinyHandle> nodeHandles;

    for (const auto& node : model.nodes) {
        nodeHandles.push_back(scene.addNodeRaw(node.name));
    }

    // Set the root node to the first node's actual handle
    // This in theory should be correct if you load the proper model
    if (!nodeHandles.empty()) scene.setRoot(nodeHandles[0]);

    // Second pass: Remap parent/child relationships and add components
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        tinyHandle nodeHandle = nodeHandles[i];

        tinyHandle parentHandle;
        std::vector<tinyHandle> childrenHandles;

        const tinyNodeRT& originalNode = model.nodes[i];

        // Remap parent handle
        if (validIndex(originalNode.parentHandle, nodeHandles)) {
            parentHandle = nodeHandles[originalNode.parentHandle.index];
        };

        // Remap children handles
        for (const tinyHandle& childHandle : originalNode.childrenHandles) {
            if (validIndex(childHandle, nodeHandles)) {
                childrenHandles.push_back(nodeHandles[childHandle.index]);
            }
        }

        scene.setNodeParent(nodeHandle, parentHandle);
        scene.setNodeChildren(nodeHandle, childrenHandles);

        // Add component with scene API to ensure proper handling
        if (originalNode.has<tinyNodeRT::T3D>()) {
            const auto* ogTransform = originalNode.get<tinyNodeRT::T3D>();
            auto* newTransform = scene.writeComp<tinyNodeRT::T3D>(nodeHandle);
            *newTransform = *ogTransform;
        }

        if (originalNode.has<tinyNodeRT::MR3D>()) {
            const auto* ogMeshRender = originalNode.get<tinyNodeRT::MR3D>();
            auto* newMeshRender = scene.writeComp<tinyNodeRT::MR3D>(nodeHandle);

            if (validIndex(ogMeshRender->pMeshHandle, glbMeshRHandle)) {
                newMeshRender->pMeshHandle = glbMeshRHandle[ogMeshRender->pMeshHandle.index];
            }

            if (validIndex(ogMeshRender->skeleNodeHandle, nodeHandles)) {
                newMeshRender->skeleNodeHandle = nodeHandles[ogMeshRender->skeleNodeHandle.index];
            }
        }

        if (originalNode.has<tinyNodeRT::BA3D>()) {
            const auto* ogBoneAttach = originalNode.get<tinyNodeRT::BA3D>();
            auto* newBoneAttach = scene.writeComp<tinyNodeRT::BA3D>(nodeHandle);

            if (validIndex(ogBoneAttach->skeleNodeHandle, nodeHandles)) {
                newBoneAttach->skeleNodeHandle = nodeHandles[ogBoneAttach->skeleNodeHandle.index];
            }

            newBoneAttach->boneIndex = ogBoneAttach->boneIndex;
        }

        if (originalNode.has<tinyNodeRT::SK3D>()) {
            const auto* ogSkeleComp = originalNode.get<tinyNodeRT::SK3D>();
            auto* newSkeleRT = scene.writeComp<tinyNodeRT::SK3D>(nodeHandle);

            if (validIndex(ogSkeleComp->pHandle, glbSkeleRHandle)) {
                // Construct new skeleton runtime from the original skeleton
                newSkeleRT->set(glbSkeleRHandle[ogSkeleComp->pHandle.index]);
            }
        }

        if (originalNode.has<tinyNodeRT::AN3D>()) {
            const auto* ogAnimeComp = originalNode.get<tinyNodeRT::AN3D>();
            auto* newAnimeComp = scene.writeComp<tinyNodeRT::AN3D>(nodeHandle);

            *newAnimeComp = model.animations[ogAnimeComp->pHandle.index];

            for (auto& anime : newAnimeComp->MAL()) {
                auto* toAnime = newAnimeComp->get(anime.second);
                if (!toAnime) continue; // Should not happen

                for (auto& channel : toAnime->channels) {
                    if (!validIndex(channel.node, nodeHandles)) continue;
                    channel.node = nodeHandles[channel.node.index];
                }
            }
        }
    }

    // Add scene to registry
    tinyHandle fnHandle = fs_->addFile(fnModelFolder, scene.name, std::move(scene));
    typeHandle tHandle = fs_->ftypeHandle(fnHandle);

    // Return the model folder handle instead of the scene handle
    return fnModelFolder;
}



void tinyProject::addSceneInstance(tinyHandle fromHandle, tinyHandle toHandle, tinyHandle parentHandle) {
    const tinySceneRT* fromScene = fs().rGet<tinySceneRT>(fromHandle);
    tinySceneRT* toScene = fs().rGet<tinySceneRT>(toHandle);
    if (toHandle == fromHandle || !toScene || !fromScene) return;

    toScene->addScene(fromScene, parentHandle);
}


void tinyProject::vkCreateSceneResources() {
    VkDevice device = deviceVK->device;

    // It needs to be dynamic to allow per-frame offsets (avoid race conditions)

    skinDescLayout.create(device, {
        {0, DescType::StorageBufferDynamic, 1, ShaderStage::Vertex, nullptr}
    });

    skinDescPool.create(device, {
        {DescType::StorageBufferDynamic, maxSkeletons}
    }, maxSkeletons);

    // Setup shared scene requirements
    sharedReq.maxFramesInFlight = 2;
    sharedReq.fsRegistry = &fs().registry();
    sharedReq.deviceVK = deviceVK;
    sharedReq.skinDescPool = skinDescPool;
    sharedReq.skinDescLayout = skinDescLayout;

    // Create dummy skin descriptor set for rigged meshes without skeleton
    createDummySkinDescriptorSet();
}

void tinyProject::createDummySkinDescriptorSet() {
    // Create dummy descriptor set (allocate from same pool as real skeletons)
    dummySkinDescSet.allocate(deviceVK->device, skinDescPool.get(), skinDescLayout.get());

    // Create dummy skin buffer with 1 identity matrix
    glm::mat4 identityMatrix = glm::mat4(1.0f);
    VkDeviceSize bufferSize = sizeof(glm::mat4);

    dummySkinBuffer
        .setDataSize(bufferSize * sharedReq.maxFramesInFlight)
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
        .setType(DescType::StorageBufferDynamic)
        .setDescCount(1)
        .setBufferInfo({ bufferInfo })
        .updateDescSets(deviceVK->device);
}