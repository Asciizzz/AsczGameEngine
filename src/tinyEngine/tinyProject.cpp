#include "tinyEngine/tinyProject.hpp"
#include "tinyEngine/tinyLoader.hpp"

using NTypes = tinyNodeRT::Types;

using namespace tinyVk;

// A quick function for range validation
template<typename T>
bool validHandle(tinyHandle handle, const std::vector<T>& vec) {
    return static_cast<bool>(handle) && handle.index < vec.size();
}

template<typename T>
bool validIndex(int index, const std::vector<T>& vec) {
    return index >= 0 && static_cast<size_t>(index) < vec.size();
}

tinyHandle linkHandle(int index, const std::vector<tinyHandle>& vec) {
    if (index < 0 || static_cast<size_t>(index) >= vec.size()) {
        return tinyHandle();
    }
    return vec[index];
}

tinyProject::tinyProject(const tinyVk::Device* deviceVk) : deviceVk_(deviceVk) {
    // Create camera and global UBO manager
    camera_ = MakeUnique<tinyCamera>(glm::vec3(0.0f, 0.0f, 0.0f), 45.0f, 0.5f, 1000.0f);
    global_ = MakeUnique<tinyGlobal>(2);
    global_->vkCreate(deviceVk_);

    setupFS();
    vkCreateResources();
    vkCreateDefault();
}

tinyProject::~tinyProject() {
    // Cleanup everything first before destroying pools
    global_.reset();
    camera_.reset();
    fs_.reset();
}



tinyHandle tinyProject::addModel(tinyModel& model, tinyHandle parentFolder) {
    parentFolder = parentFolder ? parentFolder : fs_->root();

    // Create a folder for the model in the specified parent
    tinyHandle fnModelFolder = fs_->createFolder(parentFolder, model.name);

    // Note: fnHandle - handle to file node in tinyFS's fnodes
    //       tHandle - handle to the actual data in the registry (infused with Type info for tinyFS usage)

    // Import textures to registry
    std::vector<tinyHandle> glbTexRHandle;
    if (!model.textures.empty()) {
        tinyHandle fnTexFolder = fs_->createFolder(fnModelFolder, "Textures");

        for (auto& mTexture : model.textures) {
            tinyTextureVk textureVk;
            textureVk.createFrom(std::move(mTexture.texture), deviceVk_);

            tinyHandle fnHandle = fs_->createFile(fnTexFolder, mTexture.name, std::move(textureVk));
            tinyHandle rHandle = fs_->fDataHandle(fnHandle);
            glbTexRHandle.push_back(rHandle);
        }
    }

    std::vector<tinyHandle> glmMatRHandle;
    if (!model.materials.empty()) {
        tinyHandle fnMatFolder = fs_->createFolder(fnModelFolder, "Materials");

        for (const auto& mMaterial : model.materials) {
            tinyMaterialVk materialVk;
            materialVk.init(
                deviceVk_,
                sharedRes_.defaultTextureVk0(),
                sharedRes_.defaultTextureVk1(),
                sharedRes_.matDescLayout(),
                sharedRes_.matDescPool()
            );

            // Set material base color from model
            materialVk.setBaseColor(mMaterial.baseColor);

            // Remap the material's texture indices

            // Albedo texture
            tinyHandle albHandle = linkHandle(mMaterial.albIndx, glbTexRHandle);
            materialVk.setTexture(MTexSlot::Albedo, registry().get<tinyTextureVk>(albHandle));

            // Normal texture
            tinyHandle nrmlHandle = linkHandle(mMaterial.nrmlIndx, glbTexRHandle);
            materialVk.setTexture(MTexSlot::Normal, registry().get<tinyTextureVk>(nrmlHandle));
            // Metallic texture
            tinyHandle metalHandle = linkHandle(mMaterial.metalIndx, glbTexRHandle);
            materialVk.setTexture(MTexSlot::MetalRough, registry().get<tinyTextureVk>(metalHandle));

            // Emissive texture
            tinyHandle emisHandle = linkHandle(mMaterial.emisIndx, glbTexRHandle);
            materialVk.setTexture(MTexSlot::Emissive, registry().get<tinyTextureVk>(emisHandle));

            tinyHandle fnHandle = fs_->createFile(fnMatFolder, mMaterial.name, std::move(materialVk));
            tinyHandle rHandle = fs_->fDataHandle(fnHandle);
            glmMatRHandle.push_back(rHandle);
        }
    }

    std::vector<tinyHandle> glbMeshRHandle;
    if (!model.meshes.empty()) {
        tinyHandle fnMeshFolder = fs_->createFolder(fnModelFolder, "Meshes");

        for (auto& mMesh : model.meshes) {
            // Remap material indices
            std::vector<tinyMesh::Part>& remapPart = mMesh.mesh.parts();
            for (auto& part : remapPart) {
                part.material = linkHandle(part.material.index, glmMatRHandle);
            }

            tinyMeshVk meshVk;
            meshVk.init(deviceVk_, sharedRes_.mrphDsDescLayout(), sharedRes_.mrphDsDescPool());

            meshVk.createFrom(std::move(mMesh.mesh));

            tinyHandle fnHandle = fs_->createFile(fnMeshFolder, mMesh.name, std::move(meshVk));
            tinyHandle rHandle = fs_->fDataHandle(fnHandle);
            glbMeshRHandle.push_back(rHandle);
        }
    }

    // Import skeletons to registry
    std::vector<tinyHandle> glbSkeleRHandle;

    if (!model.skeletons.empty()) {
        tinyHandle fnSkeleFolder = fs_->createFolder(fnModelFolder, "Skeletons");
        for (auto& mSkeleton : model.skeletons) {

            tinyHandle fnHandle = fs_->createFile(fnSkeleFolder, mSkeleton.name, std::move(mSkeleton.skeleton));
            tinyHandle rHandle = fs_->fDataHandle(fnHandle);
            glbSkeleRHandle.push_back(rHandle);
        }
    }

    // If scene has no nodes, return early
    if (model.nodes.empty()) return tinyHandle();

    // Create scene with nodes - preserve hierarchy but remap resource references
    tinySceneRT scene;
    scene.setSharedRes(sharedRes_);

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

        // const tinyNodeRT& originalNode = model.nodes[i];
        const tinyModel::Node& ogNode = model.nodes[i];

        // Remap parent handle
        if (validIndex(ogNode.parent, nodeHandles)) {
            parentHandle = nodeHandles[ogNode.parent];
        };

        // Remap children handles
        for (int child : ogNode.children) {
            if (validIndex(child, nodeHandles)) {
                childrenHandles.push_back(nodeHandles[child]);
            }
        }

        scene.setNodeParent(nodeHandle, parentHandle);
        scene.setNodeChildren(nodeHandle, childrenHandles);

        if (ogNode.hasTRFM3D()) {
            auto* newTransform = scene.writeComp<tinyNodeRT::TRFM3D>(nodeHandle);
            newTransform->init(ogNode.TRFM3D);
        }

        if (ogNode.hasMESHR()) {
            auto* newMeshRender = scene.writeComp<tinyNodeRT::MESHRD>(nodeHandle);

            if (validIndex(ogNode.MESHRD_meshIndx, glbMeshRHandle)) {
                newMeshRender->setMesh(glbMeshRHandle[ogNode.MESHRD_meshIndx]);
            }

            if (validIndex(ogNode.MESHRD_skeleNodeIndx, nodeHandles)) {
                newMeshRender->setSkeleNode(nodeHandles[ogNode.MESHRD_skeleNodeIndx]);
            }
        }

        if (ogNode.hasBONE3D()) {
            auto* newBoneAttach = scene.writeComp<tinyNodeRT::BONE3D>(nodeHandle);

            if (validIndex(ogNode.BONE3D_skeleNodeIndx, nodeHandles)) {
                newBoneAttach->skeleNodeHandle = nodeHandles[ogNode.BONE3D_skeleNodeIndx];
            }

            newBoneAttach->boneIndex = ogNode.BONE3D_boneIndx;
        }

        if (ogNode.hasSKEL3D()) {
            auto* newSkeleRT = scene.writeComp<tinyNodeRT::SKEL3D>(nodeHandle);

            if (validIndex(ogNode.SKEL3D_skeleIndx, glbSkeleRHandle)) {
                // Construct new skeleton runtime from the original skeleton
                newSkeleRT->set(glbSkeleRHandle[ogNode.SKEL3D_skeleIndx]);
            }
        }

        if (ogNode.hasANIM3D()) {
            auto* newAnimeComp = scene.writeComp<tinyNodeRT::ANIM3D>(nodeHandle);

            *newAnimeComp = model.animations[ogNode.ANIM3D_animeIndx];

            for (auto& anime : newAnimeComp->MAL()) {
                auto* toAnime = newAnimeComp->get(anime.second);
                if (!toAnime) continue; // Should not happen

                for (auto& channel : toAnime->channels) {
                    if (!validHandle(channel.node, nodeHandles)) continue;
                    channel.node = nodeHandles[channel.node.index];
                }
            }
        }
    }

    // Add scene to registry
    tinyHandle fnHandle = fs_->createFile(fnModelFolder, model.name, std::move(scene));
    return fs_->fDataHandle(fnHandle); // Return the scene's registry handle
}

void tinyProject::addSceneInstance(tinyHandle fromHandle, tinyHandle toHandle, tinyHandle parentHandle) {
    if (fromHandle == toHandle) return; // Prevent self-copy

    if (tinySceneRT* toScene = registry().get<tinySceneRT>(toHandle)) {
        toScene->addScene(fromHandle, parentHandle);
    }
}

// ------------------- Filesystem Setup -------------------

void tinyProject::setupFS() {
    fs_ = MakeUnique<tinyFS>();

    // ------------------ Standard files ------------------

    tinyFS::TypeInfo* ascn = fs_->typeInfo<tinySceneRT>();
    ascn->ext = "ascn";
    ascn->color[0] = 102; ascn->color[1] = 255; ascn->color[2] = 102;

    tinyFS::TypeInfo* amat = fs_->typeInfo<tinyMaterialVk>();
    amat->ext = "amat";
    amat->color[0] = 255; amat->color[1] = 102; amat->color[2] = 255;

    tinyFS::TypeInfo* atex = fs_->typeInfo<tinyTextureVk>();
    atex->ext = "atex";
    atex->color[0] = 102; atex->color[1] = 102; atex->color[2] = 255;

    tinyFS::TypeInfo* amsh = fs_->typeInfo<tinyMeshVk>();
    amsh->ext = "amsh";
    amsh->color[0] = 255; amsh->color[1] = 255; amsh->color[2] = 102;

    tinyFS::TypeInfo* askl = fs_->typeInfo<tinySkeleton>();
    askl->ext = "askl";
    askl->color[0] = 102; askl->color[1] = 255; askl->color[2] = 255;

    tinyFS::TypeInfo* ascr = fs_->typeInfo<tinyScript>();
    ascr->ext = "ascr";
    ascr->color[0] = 204; ascr->color[1] = 204; ascr->color[2] = 51;

    // ------------------- Special "files" -------------------

    // Resources that lives in the registry but not registered (pun intended)
    // as files in the filesystem

    // tinyFS::TypeInfo* descPool = fs_->typeInfo<tinyVk::DescPool>();
    // descPool->priority = UINT8_MAX; // Delete last
    // descPool->safeDelete = true;

    // tinyFS::TypeInfo* descLayout = fs_->typeInfo<tinyVk::DescSLayout>();
    // descLayout->priority = UINT8_MAX; // Delete last
    // descLayout->safeDelete = true;

    // tinyFS::TypeInfo* descSet = fs_->typeInfo<tinyVk::DescSet>();
    // descSet->priority = UINT8_MAX - 1; // Delete before pool/layout

    // tinyFS::TypeInfo* dataBuffer = fs_->typeInfo<tinyVk::DataBuffer>();
    // dataBuffer->priority = UINT8_MAX - 2; // Delete before desc sets

    // // ------------------ Other useful files ------------------
    tinyFS::TypeInfo* atxt = fs_->typeInfo<tinyText>();
    atxt->ext = "txt";
    atxt->color[0] = 153; atxt->color[1] = 153; atxt->color[2] = 153;
}


// ------------------ Vulkan Resource Creation ------------------

void tinyProject::vkCreateResources() {
    VkDevice device = deviceVk_->device;

    // Setup shared scene requirements
    sharedRes_.maxFramesInFlight = 2;
    sharedRes_.fsRegistry = &fs().registry();
    sharedRes_.deviceVk = deviceVk_;

    // Skin descriptors
    DescSLayout skinDescLayout;
    skinDescLayout.create(device, { {0, DescType::StorageBufferDynamic, 1, ShaderStage::Vertex, nullptr} });
    sharedRes_.hSkinDescLayout = rAdd<DescSLayout>(std::move(skinDescLayout));

    DescPool skinDescPool;
    skinDescPool.create(device, { {DescType::StorageBufferDynamic, maxSkeletons} }, maxSkeletons);
    sharedRes_.hSkinDescPool = rAdd<DescPool>(std::move(skinDescPool));

    // Material descriptors
    DescSLayout matDescLayout = tinyMaterialVk::createDescSetLayout(device);
    DescPool matDescPool = tinyMaterialVk::createDescPool(device, maxMaterials);
    sharedRes_.hMatDescLayout = rAdd<DescSLayout>(std::move(matDescLayout));
    sharedRes_.hMatDescPool = rAdd<DescPool>(std::move(matDescPool));

    // Mesh morph delta descriptors
    DescSLayout meshMrphDsDescLayout = tinyMeshVk::createMrphDescSetLayout(device, false);
    DescPool meshMrphDsDescPool = tinyMeshVk::createMrphDescPool(device, maxMeshes, false);
    sharedRes_.hMrphDsDescLayout = rAdd<DescSLayout>(std::move(meshMrphDsDescLayout));
    sharedRes_.hMrphDsDescPool = rAdd<DescPool>(std::move(meshMrphDsDescPool));

    // Mesh morph weight descriptors
    DescSLayout meshMrphWsDescLayout = tinyMeshVk::createMrphDescSetLayout(device);
    DescPool meshMrphWsDescPool = tinyMeshVk::createMrphDescPool(device, maxMeshes);
    sharedRes_.hMrphWsDescLayout = rAdd<DescSLayout>(std::move(meshMrphWsDescLayout));
    sharedRes_.hMrphWsDescPool = rAdd<DescPool>(std::move(meshMrphWsDescPool));
}

void tinyProject::vkCreateDefault() {

//  ---------- Create default material and texture ----------

    tinyTextureVk defaultTextureVk0 = tinyTextureVk::aPixel(deviceVk_, 255, 255, 255, 255);
    sharedRes_.hDefaultTextureVk0 = rAdd<tinyTextureVk>(std::move(defaultTextureVk0));

    tinyTextureVk defaultTextureVk1 = tinyTextureVk::aPixel(deviceVk_, 0, 0, 0, 0);
    sharedRes_.hDefaultTextureVk1 = rAdd<tinyTextureVk>(std::move(defaultTextureVk1));

    tinyMaterialVk defaultMaterialVk;
    defaultMaterialVk.init(
        deviceVk_,
        sharedRes_.defaultTextureVk0(),
        sharedRes_.defaultTextureVk1(),
        sharedRes_.matDescLayout(),
        sharedRes_.matDescPool()
    );

    sharedRes_.hDefaultMaterialVk = rAdd<tinyMaterialVk>(std::move(defaultMaterialVk));

//  -------------- Create dummy skin resources --------------

    DataBuffer dummySkinBuffer;
    DescSet dummySkinDescSet;

    dummySkinDescSet.allocate(deviceVk_->device, sharedRes_.skinDescPool(), sharedRes_.skinDescLayout());
    tinyRT_SKEL3D::vkWrite(deviceVk_, &dummySkinBuffer, &dummySkinDescSet, sharedRes_.maxFramesInFlight, 1);

    rAdd<DataBuffer>(std::move(dummySkinBuffer)); // No need to store handle
    sharedRes_.hDummySkinDescSet = rAdd<DescSet>(std::move(dummySkinDescSet));

// -------------- Create dummy morph target resources --------------

    DataBuffer dummyMrphDsBuffer, dummyMrphWsBuffer;
    DescSet dummyMrphDsDescSet, dummyMrphWsDescSet;

    dummyMrphDsDescSet.allocate(deviceVk_->device, sharedRes_.mrphDsDescPool(), sharedRes_.mrphDsDescLayout());
    tinyRT_MESHRD::vkWrite(deviceVk_, &dummyMrphDsBuffer, &dummyMrphDsDescSet, 1, 1); // Non-dynamic

    dummyMrphWsDescSet.allocate(deviceVk_->device, sharedRes_.mrphWsDescPool(), sharedRes_.mrphWsDescLayout());
    tinyRT_MESHRD::vkWrite(deviceVk_, &dummyMrphWsBuffer, &dummyMrphWsDescSet, sharedRes_.maxFramesInFlight, 1);

    rAdd<DataBuffer>(std::move(dummyMrphDsBuffer));
    sharedRes_.hDummyMeshMrphDsDescSet = rAdd<DescSet>(std::move(dummyMrphDsDescSet));

    rAdd<DataBuffer>(std::move(dummyMrphWsBuffer));
    sharedRes_.hDummyMeshMrphWsDescSet = rAdd<DescSet>(std::move(dummyMrphWsDescSet));

// ------------------ Create Main Scene ------------------

// CRITICAL: Main Scene must be created last after all resources are ready

    tinySceneRT mainScene;
    mainScene.addRoot("Root");
    mainScene.setSharedRes(sharedRes_);

    tinyHandle mainSceneFileHandle = fs_->createFile(fs_->root(), "Main Scene", std::move(mainScene));

    mainSceneHandle = fs_->fDataHandle(mainSceneFileHandle);
}