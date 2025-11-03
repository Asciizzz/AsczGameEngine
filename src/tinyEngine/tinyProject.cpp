#include "tinyEngine/tinyProject.hpp"
#include "tinyEngine/tinyLoader.hpp"

using NTypes = tinyNodeRT::Types;

using namespace tinyVk;

// A quick function for range validation
template<typename T>
bool validHandle(tinyHandle handle, const std::vector<T>& vec) {
    return handle.valid() && handle.index < vec.size();
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
    camera_ = MakeUnique<tinyCamera>(glm::vec3(0.0f, 0.0f, 0.0f), 45.0f, 0.1f, 1000.0f);
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
        tinyTextureVk textureVk;
        textureVk.createFrom(std::move(texture), deviceVk_);

        tinyHandle fnHandle = fs_->addFile(fnTexFolder, textureVk.name(), std::move(textureVk));
        typeHandle tHandle = fs_->fTypeHandle(fnHandle);

        glbTexRHandle.push_back(tHandle.handle);
    }

    // Import materials to registry with remapped texture references

    std::vector<tinyHandle> glmMatRHandle;
    for (const auto& material : model.materials) {
        tinyMaterialVk materialVk;
        materialVk.init(deviceVk_, sharedRes_.defaultTextureVk(), sharedRes_.matDescLayout(), sharedRes_.matDescPool());

        materialVk.name = material.name;

        // Set material base color from model
        materialVk.setBaseColor(material.baseColor);

        // Remap the material's texture indices

        // Albedo texture
        tinyHandle albHandle = linkHandle(material.albIndx, glbTexRHandle);
        materialVk.setTexture(MTexSlot::Albedo, fs_->rGet<tinyTextureVk>(albHandle));

        // Normal texture
        tinyHandle nrmlHandle = linkHandle(material.nrmlIndx, glbTexRHandle);
        materialVk.setTexture(MTexSlot::Normal, fs_->rGet<tinyTextureVk>(nrmlHandle));

        // Metallic texture
        tinyHandle metalHandle = linkHandle(material.metalIndx, glbTexRHandle);
        materialVk.setTexture(MTexSlot::MetalRough, fs_->rGet<tinyTextureVk>(metalHandle));

        // Emissive texture
        tinyHandle emisHandle = linkHandle(material.emisIndx, glbTexRHandle);
        materialVk.setTexture(MTexSlot::Emissive, fs_->rGet<tinyTextureVk>(emisHandle));

        // Add material to fsRegistry
        tinyHandle fnHandle = fs_->addFile(fnMatFolder, materialVk.name, std::move(materialVk));
        typeHandle tHandle = fs_->fTypeHandle(fnHandle);

        glmMatRHandle.push_back(tHandle.handle);
    }

    // Import meshes to registry with remapped material references
    std::vector<tinyHandle> glbMeshRHandle;
    for (auto& mesh : model.meshes) {
        // Remap material indices
        std::vector<tinyMesh::Part>& remapPart = mesh.parts();
        for (auto& part : remapPart) {
            part.material = linkHandle(part.material.index, glmMatRHandle);
        }

        tinyMeshVk meshVk;
        meshVk.init(deviceVk_, sharedRes_.mrphDsDescLayout(), sharedRes_.mrphDsDescPool());

        meshVk.createFrom(std::move(mesh));

        tinyHandle fnHandle = fs_->addFile(fnMeshFolder, mesh.name, std::move(meshVk));
        typeHandle tHandle = fs_->fTypeHandle(fnHandle);

        glbMeshRHandle.push_back(tHandle.handle);
    }

    // Import skeletons to registry
    std::vector<tinyHandle> glbSkeleRHandle;
    for (auto& skeleton : model.skeletons) {
        tinyHandle fnHandle = fs_->addFile(fnSkeleFolder, skeleton.name, std::move(skeleton));
        typeHandle tHandle = fs_->fTypeHandle(fnHandle);

        glbSkeleRHandle.push_back(tHandle.handle);
    }

    // If scene has no nodes, return early
    if (model.nodes.empty()) return fnModelFolder;

    // Create scene with nodes - preserve hierarchy but remap resource references
    tinySceneRT scene(model.name);
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

        auto* newTransform = scene.writeComp<tinyNodeRT::TRFM3D>(nodeHandle);
        newTransform->init(ogNode.TRFM3D);

        if (ogNode.hasMESHR()) {
            auto* newMeshRender = scene.writeComp<tinyNodeRT::MESHRD>(nodeHandle);

            if (validIndex(ogNode.MESHR_meshIndx, glbMeshRHandle)) {
                newMeshRender->setMesh(glbMeshRHandle[ogNode.MESHR_meshIndx]);
            }

            if (validIndex(ogNode.MESHR_skeleNodeIndx, nodeHandles)) {
                newMeshRender->setSkeleNode(nodeHandles[ogNode.MESHR_skeleNodeIndx]);
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
    tinyHandle fnHandle = fs_->addFile(fnModelFolder, scene.name, std::move(scene));
    typeHandle tHandle = fs_->fTypeHandle(fnHandle);

    // Return the model folder handle instead of the scene handle
    return fnModelFolder;
}

void tinyProject::addSceneInstance(tinyHandle fromHandle, tinyHandle toHandle, tinyHandle parentHandle) {
    const tinySceneRT* fromScene = fs().rGet<tinySceneRT>(fromHandle);
    tinySceneRT* toScene = fs().rGet<tinySceneRT>(toHandle);
    if (toHandle == fromHandle || !toScene || !fromScene) return;

    toScene->addScene(fromScene, parentHandle);
}

// ------------------- Filesystem Setup -------------------

void tinyProject::setupFS() {
    fs_ = MakeUnique<tinyFS>();

    // ------------------ Standard files ------------------

    tinyFS::TypeInfo* ascn = fs_->typeInfo<tinySceneRT>();
    ascn->typeExt = tinyFS::TypeExt("ascn", 0.4f, 1.0f, 0.4f);

    tinyFS::TypeInfo* amat = fs_->typeInfo<tinyMaterialVk>();
    amat->typeExt = tinyFS::TypeExt("amat", 1.0f, 0.4f, 1.0f);

    tinyFS::TypeInfo* atex = fs_->typeInfo<tinyTextureVk>();
    atex->typeExt = tinyFS::TypeExt("atex", 0.4f, 0.4f, 1.0f);
    atex->priority = 1; // Material delete before texture

    atex->setRmRule<tinyTextureVk>([](const tinyTextureVk& tex) -> bool {
        return tex.useCount() == 0;
    });

    tinyFS::TypeInfo* amsh = fs_->typeInfo<tinyMeshVk>();
    amsh->typeExt = tinyFS::TypeExt("amsh", 1.0f, 1.0f, 0.4f);

    tinyFS::TypeInfo* askl = fs_->typeInfo<tinySkeleton>();
    askl->typeExt = tinyFS::TypeExt("askl", 0.4f, 1.0f, 1.0f);
    askl->safeDelete = true; // Skeletons are just data, safe to delete

    // ------------------- Special "files" -------------------

    // Resources that lives in the registry but not registered (pun intended)
    // as files in the filesystem

    tinyFS::TypeInfo* descPool = fs_->typeInfo<tinyVk::DescPool>();
    descPool->priority = UINT8_MAX; // Delete last
    descPool->safeDelete = true;

    tinyFS::TypeInfo* descLayout = fs_->typeInfo<tinyVk::DescSLayout>();
    descLayout->priority = UINT8_MAX; // Delete last
    descLayout->safeDelete = true;

    tinyFS::TypeInfo* descSet = fs_->typeInfo<tinyVk::DescSet>();
    descSet->priority = UINT8_MAX - 1; // Delete before pool/layout

    tinyFS::TypeInfo* dataBuffer = fs_->typeInfo<tinyVk::DataBuffer>();
    dataBuffer->priority = UINT8_MAX - 2; // Delete before desc sets
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
    sharedRes_.hSkinDescLayout = fsAdd<DescSLayout>(std::move(skinDescLayout));

    DescPool skinDescPool;
    skinDescPool.create(device, { {DescType::StorageBufferDynamic, maxSkeletons} }, maxSkeletons);
    sharedRes_.hSkinDescPool = fsAdd<DescPool>(std::move(skinDescPool));

    // Material descriptors
    DescSLayout matDescLayout = tinyMaterialVk::createDescSetLayout(device);
    DescPool matDescPool = tinyMaterialVk::createDescPool(device, maxMaterials);
    sharedRes_.hMatDescLayout = fsAdd<DescSLayout>(std::move(matDescLayout));
    sharedRes_.hMatDescPool = fsAdd<DescPool>(std::move(matDescPool));

    // Mesh morph delta descriptors
    DescSLayout meshMrphDsDescLayout = tinyMeshVk::createMrphDescSetLayout(device, false);
    DescPool meshMrphDsDescPool = tinyMeshVk::createMrphDescPool(device, maxMeshes, false);
    sharedRes_.hMrphDsDescLayout = fsAdd<DescSLayout>(std::move(meshMrphDsDescLayout));
    sharedRes_.hMrphDsDescPool = fsAdd<DescPool>(std::move(meshMrphDsDescPool));

    // Mesh morph weight descriptors
    DescSLayout meshMrphWsDescLayout = tinyMeshVk::createMrphDescSetLayout(device);
    DescPool meshMrphWsDescPool = tinyMeshVk::createMrphDescPool(device, maxMeshes);
    sharedRes_.hMrphWsDescLayout = fsAdd<DescSLayout>(std::move(meshMrphWsDescLayout));
    sharedRes_.hMrphWsDescPool = fsAdd<DescPool>(std::move(meshMrphWsDescPool));
}

void tinyProject::vkCreateDefault() {

// ------------------ Create Main Scene ------------------

    tinySceneRT mainScene("Main Scene");
    mainScene.addRoot("Root");
    mainScene.setSharedRes(sharedRes_);

    // Create "Main Scene" as a non-deletable file in root directory
    tinyFS::Node::CFG sceneConfig;
    sceneConfig.deletable = false;

    tinyHandle mainSceneFileHandle = fs_->addFile(fs_->rootHandle(), "Main Scene", std::move(mainScene), sceneConfig);
    typeHandle mainScenetypeHandle = fs_->fTypeHandle(mainSceneFileHandle);

    initialSceneHandle = mainScenetypeHandle.handle; // Store the initial scene handle

//  ---------- Create default material and texture ----------

    tinyTextureVk defaultTextureVk = tinyTextureVk::defaultTexture(deviceVk_);
    sharedRes_.hDefaultTextureVk = fsAdd<tinyTextureVk>(std::move(defaultTextureVk));

    tinyMaterialVk defaultMaterialVk;
    defaultMaterialVk.init(deviceVk_, sharedRes_.defaultTextureVk(), sharedRes_.matDescLayout(), sharedRes_.matDescPool());

    sharedRes_.hDefaultMaterialVk = fsAdd<tinyMaterialVk>(std::move(defaultMaterialVk));

//  -------------- Create dummy skin resources --------------

    dummySkinDescSet.allocate(deviceVk_->device, sharedRes_.skinDescPool(), sharedRes_.skinDescLayout());
    tinyRT_SKEL3D::vkWrite(deviceVk_, &dummySkinBuffer, &dummySkinDescSet, sharedRes_.maxFramesInFlight, 1);

// -------------- Create dummy morph target resources --------------

    dummyMrphDsDescSet.allocate(deviceVk_->device, sharedRes_.mrphDsDescPool(), sharedRes_.mrphDsDescLayout());
    tinyRT_MESHRD::vkWrite(deviceVk_, &dummyMrphDsBuffer, &dummyMrphDsDescSet, 1, 1); // Non-dynamic

    dummyMrphWsDescSet.allocate(deviceVk_->device, sharedRes_.mrphWsDescPool(), sharedRes_.mrphWsDescLayout());
    tinyRT_MESHRD::vkWrite(deviceVk_, &dummyMrphWsBuffer, &dummyMrphWsDescSet, sharedRes_.maxFramesInFlight, 1);

}