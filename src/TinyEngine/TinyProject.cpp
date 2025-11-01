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

tinyProject::tinyProject(const tinyVk::Device* deviceVk) : deviceVk(deviceVk) {
    // Create camera and global UBO manager
    camera_ = MakeUnique<tinyCamera>(glm::vec3(0.0f, 0.0f, 0.0f), 45.0f, 0.1f, 1000.0f);
    global_ = MakeUnique<tinyGlobal>(2);
    global_->vkCreate(deviceVk);

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
        textureVk.createFrom(std::move(texture), deviceVk);

        tinyHandle fnHandle = fs_->addFile(fnTexFolder, textureVk.name(), std::move(textureVk));
        typeHandle tHandle = fs_->fTypeHandle(fnHandle);

        glbTexRHandle.push_back(tHandle.handle);
    }

    // Import materials to registry with remapped texture references

    std::vector<tinyHandle> glmMatRHandle;
    for (const auto& material : model.materials) {
        tinyMaterialVk materialVk;
        materialVk.init(deviceVk, &defaultTextureVk, matDescLayout, matDescPool);

        materialVk.name = material.name;

        // Set material base color from model
        materialVk.setBaseColor(material.baseColor);

        // Remap the material's texture indices

        // Albedo texture
        tinyHandle albHandle = linkHandle(material.albIndx, glbTexRHandle);
        materialVk.setAlbTex(fs_->rGet<tinyTextureVk>(albHandle));

        // Normal texture
        tinyHandle nrmlHandle = linkHandle(material.nrmlIndx, glbTexRHandle);
        materialVk.setNrmlTex(fs_->rGet<tinyTextureVk>(nrmlHandle));

        // Metallic texture
        tinyHandle metalHandle = linkHandle(material.metalIndx, glbTexRHandle);
        materialVk.setMetalTex(fs_->rGet<tinyTextureVk>(metalHandle));

        // Emissive texture
        tinyHandle emisHandle = linkHandle(material.emisIndx, glbTexRHandle);
        materialVk.setEmisTex(fs_->rGet<tinyTextureVk>(emisHandle));

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
        meshVk.init(deviceVk, meshMrphDsDescLayout, meshMrphDsDescPool);

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
}


// ------------------ Vulkan Resource Creation ------------------

void tinyProject::vkCreateResources() {
    VkDevice device = deviceVk->device;

    skinDescLayout.create(device, {
        {0, DescType::StorageBufferDynamic, 1, ShaderStage::Vertex, nullptr}
    });

    skinDescPool.create(device, {
        {DescType::StorageBufferDynamic, maxSkeletons}
    }, maxSkeletons);

    matDescLayout = tinyMaterialVk::createDescSetLayout(device);
    matDescPool = tinyMaterialVk::createDescPool(device, maxMaterials);

    meshMrphDsDescLayout = tinyMeshVk::createMrphDescSetLayout(device, false);
    meshMrphDsDescPool = tinyMeshVk::createMrphDescPool(device, maxMeshes, false);

    meshMrphWsDescLayout = tinyMeshVk::createMrphDescSetLayout(device);
    meshMrphWsDescPool = tinyMeshVk::createMrphDescPool(device, maxMeshes);

    // Setup shared scene requirements
    sharedReq.maxFramesInFlight = 2;
    sharedReq.fsRegistry = &fs().registry();
    sharedReq.deviceVk = deviceVk;
    sharedReq.skinDescPool = skinDescPool;
    sharedReq.skinDescLayout = skinDescLayout;
    sharedReq.mrphWsDescPool = meshMrphWsDescPool;
    sharedReq.mrphWsDescLayout = meshMrphWsDescLayout;
}

void tinyProject::vkCreateDefault() {

// ------------------ Create Main Scene ------------------

    tinySceneRT mainScene("Main Scene");
    mainScene.addRoot("Root");
    mainScene.setSceneReq(sceneReq());

    // Create "Main Scene" as a non-deletable file in root directory
    tinyFS::Node::CFG sceneConfig;
    sceneConfig.deletable = false;

    tinyHandle mainSceneFileHandle = fs_->addFile(fs_->rootHandle(), "Main Scene", std::move(mainScene), sceneConfig);
    typeHandle mainScenetypeHandle = fs_->fTypeHandle(mainSceneFileHandle);
    initialSceneHandle = mainScenetypeHandle.handle; // Store the initial scene handle

//  ---------- Create default material and texture ----------

    defaultTextureVk = tinyTextureVk::defaultTexture(deviceVk);
    defaultMaterialVk.init(deviceVk, &defaultTextureVk, matDescLayout, matDescPool);
    defaultMaterialVk.name = "Default Material";

//  -------------- Create dummy skin resources --------------

    dummySkinDescSet.allocate(deviceVk->device, skinDescPool.get(), skinDescLayout.get());
    tinyRT_SKEL3D::vkWrite(deviceVk, &dummySkinBuffer, &dummySkinDescSet, sharedReq.maxFramesInFlight, 1);

// -------------- Create dummy morph target resources --------------

    dummyMrphDsDescSet.allocate(deviceVk->device, meshMrphDsDescPool.get(), meshMrphDsDescLayout.get());
    tinyRT_MESHRD::vkWrite(deviceVk, &dummyMrphDsBuffer, &dummyMrphDsDescSet, 1, 1); // Non-dynamic

    dummyMrphWsDescSet.allocate(deviceVk->device, meshMrphWsDescPool.get(), meshMrphWsDescLayout.get());
    tinyRT_MESHRD::vkWrite(deviceVk, &dummyMrphWsBuffer, &dummyMrphWsDescSet, sharedReq.maxFramesInFlight, 1);

}