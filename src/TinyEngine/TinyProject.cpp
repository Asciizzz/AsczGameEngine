#include "TinyEngine/TinyProject.hpp"
#include "TinyEngine/TinyLoader.hpp"

using NTypes = TinyNode::Types;

using namespace TinyVK;

// A quick function for range validation
template<typename T>
bool isValidIndex(int index, const std::vector<T>& vec) {
    return index >= 0 && index < static_cast<int>(vec.size());
}

TinyProject::TinyProject(const TinyVK::Device* deviceVK) : deviceVK(deviceVK) {
    tinyFS = MakeUnique<TinyFS>();

    tinyFS->setTypeExt<TinyScene>("ascn");
    tinyFS->setTypeExt<TinyTexture>("atex");
    tinyFS->setTypeExt<TinyRMaterial>("amat"); // Soon to be replaced with TinyMaterial
    tinyFS->setTypeExt<TinyMesh>("amsh");
    tinyFS->setTypeExt<TinySkeleton>("askl");
    tinyFS->setTypeExt<TinyAnimation>("anim");

    vkCreateSkinResource();

    // Create Main Scene (the active scene with a single root node)
    TinyScene mainScene;
    mainScene.name = "Main Scene";
    mainScene.addRoot("Root");

    // Create "Main Scene" as a non-deletable file in root directory
    TinyFS::Node::CFG sceneConfig;
    sceneConfig.deletable = false; // Make it non-deletable

    TinyHandle mainSceneFileHandle = tinyFS->addFile(tinyFS->rootHandle(), "Main Scene", std::move(&mainScene), sceneConfig);
    TypeHandle mainSceneTypeHandle = tinyFS->getTHandle(mainSceneFileHandle);
    initialSceneHandle = mainSceneTypeHandle.handle; // Store the initial scene handle

    // Create camera and global UBO manager
    tinyCamera = MakeUnique<TinyCamera>(glm::vec3(0.0f, 0.0f, 0.0f), 45.0f, 0.1f, 1000.0f);
    tinyGlobal = MakeUnique<TinyGlobal>(2);
    tinyGlobal->createVkResources(deviceVK);

    // Create default material and texture
    TinyTexture defaultTexture = TinyTexture::createDefaultTexture();
    defaultTexture.vkCreate(deviceVK);

    defaultTextureHandle = tinyFS->addToRegistry(defaultTexture).handle;

    TinyRMaterial defaultMaterial;
    defaultMaterial.setAlbTexIndex(0);
    defaultMaterial.setNrmlTexIndex(0);

    defaultMaterialHandle = tinyFS->addToRegistry(defaultMaterial).handle;
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
        TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

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
        TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

        glmMatRHandle.push_back(tHandle.handle);
    }

    // Import meshes to registry with remapped material references
    std::vector<TinyHandle> glbMeshRHandle;
    for (auto& mesh : model.meshes) {
        // Remap submeshes' material indices
        std::vector<TinySubmesh> remappedSubmeshes = mesh.submeshes;
        for (auto& submesh : remappedSubmeshes) {
            bool valid = isValidIndex(submesh.material.index, glmMatRHandle);
            submesh.material = valid ? glmMatRHandle[submesh.material.index] : TinyHandle();
        }

        mesh.vkCreate(deviceVK);

        mesh.setSubmeshes(remappedSubmeshes);

        TinyHandle fnHandle = tinyFS->addFile(fnMeshFolder, mesh.name, std::move(&mesh));
        TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

        glbMeshRHandle.push_back(tHandle.handle);
    }

    // Import skeletons to registry
    std::vector<TinyHandle> glbSkeleRHandle;
    for (auto& skeleton : model.skeletons) {
        TinyHandle fnHandle = tinyFS->addFile(fnSkeleFolder, skeleton.name, std::move(&skeleton));
        TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

        glbSkeleRHandle.push_back(tHandle.handle);
    }

    // Create scene with nodes - preserve hierarchy but remap resource references
    TinyScene scene;
    scene.name = model.name;

    // First pass: Insert all nodes and collect their actual handles
    std::vector<TinyHandle> nodeHandles;
    nodeHandles.reserve(model.nodes.size());

    for (const auto& node : model.nodes) {
        TinyNode rtNode = node; // Copy node data

        // Remap MeshRender component's mesh reference
        if (rtNode.hasType(NTypes::MeshRender)) {
            auto* meshRenderComp = rtNode.get<TinyNode::MeshRender>();
            if (meshRenderComp)
                meshRenderComp->meshHandle = glbMeshRHandle[meshRenderComp->meshHandle.index];
        }

        // Remap Skeleton component's registry reference
        if (rtNode.hasType(NTypes::Skeleton)) {
            auto* skeletonComp = rtNode.get<TinyNode::Skeleton>();
            if (skeletonComp)
                skeletonComp->skeleHandle = glbSkeleRHandle[skeletonComp->skeleHandle.index];
        }

        // Insert and capture the actual handle returned by the pool
        TinyHandle actualHandle = scene.nodes.add(rtNode);
        nodeHandles.push_back(actualHandle);
    }

    // Set the root node to the first node's actual handle
    if (!nodeHandles.empty()) {
        scene.rootHandle = nodeHandles[0];
    }

    // Second pass: Remap parent/child relationships using actual handles
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        TinyNode* rtNode = scene.nodes.get(nodeHandles[i]);
        if (!rtNode) continue;

        const TinyNode& originalNode = model.nodes[i];

        // Clear existing children since we'll rebuild them with correct handles
        rtNode->childrenHandles.clear();

        // Remap parent handle
        if (originalNode.parentHandle.valid() && 
            originalNode.parentHandle.index < nodeHandles.size()) {
            rtNode->parentHandle = nodeHandles[originalNode.parentHandle.index];
        } else {
            rtNode->parentHandle = TinyHandle(); // Invalid handle for root
        }

        // Remap children handles
        for (const TinyHandle& childHandle : originalNode.childrenHandles) {
            if (childHandle.valid() && childHandle.index < nodeHandles.size()) {
                rtNode->childrenHandles.push_back(nodeHandles[childHandle.index]);
            }
        }
    }

    // Add scene to registry
    TinyHandle fnHandle = tinyFS->addFile(fnModelFolder, scene.name, &scene);
    TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

    // Return the model folder handle instead of the scene handle
    return fnModelFolder;
}



void TinyProject::addSceneInstance(TinyHandle fromHandle, TinyHandle toHandle, TinyHandle parentHandle) {
    auto& registry = registryRef();
    const TinyScene* sourceScene = registry.get<TinyScene>(fromHandle);
    TinyScene* targetScene = registry.get<TinyScene>(toHandle);
    
    if (!sourceScene || !targetScene) return;

    // Use root node if no valid parent provided
    TinyHandle actualParent = parentHandle.valid() ? parentHandle : targetScene->rootHandle;
    
    // Use the scene's addScene method to merge the source scene into target scene
    targetScene->addScene(*sourceScene, actualParent);
    
    // Update transforms for the entire target scene
    targetScene->updateGlbTransform();
}



void TinyProject::updateSkin(const std::vector<glm::mat4>& skinData, uint32_t frameIndex) {
    if (frameIndex >= skinBuffers.size()) return; // Invalid frame index

    // Copy skin data to the appropriate buffer
    skinBuffers[frameIndex].copyData(skinData.data(), sizeof(glm::mat4) * skinData.size());
}



void TinyProject::vkCreateSkinResource() {
    VkDeviceSize bufferSize = sizeof(glm::mat4) * maxSkinMatrices;
    std::vector<glm::mat4> singleBone = { glm::mat4(1.0f) };

    skinDescLayout.create(*deviceVK, {
        {0, DescType::StorageBuffer, 1, ShaderStage::Vertex, nullptr}
    });

    skinDescPool.create(*deviceVK, {
        {DescType::StorageBuffer, 1}
    }, 2);

    skinDescSets.clear();
    skinBuffers.clear();

    for (size_t i = 0; i < 2; ++i) { // Assuming 2 frames in flight
        // Create buffer
        DataBuffer skinBuffer;
        skinBuffer
            .setDataSize(bufferSize)
            .setUsageFlags(BufferUsage::Storage)
            .setMemPropFlags(MemProp::HostVisibleAndCoherent)
            .createBuffer(deviceVK)
            .mapAndCopy(singleBone.data());

        skinBuffers.push_back(std::move(skinBuffer));

        // Create descriptor set
        DescSet descSet;
        descSet.allocate(*deviceVK, skinDescPool.get(), skinDescLayout.get());

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = skinBuffers.back(); // Avoid dangling reference
        bufferInfo.offset = 0;
        bufferInfo.range  = bufferSize;

        DescWrite()
            .setDstSet(descSet)
            .setType(DescType::StorageBuffer)
            .setDescCount(1)
            .setBufferInfo({ bufferInfo })
            .updateDescSets(deviceVK->device);

        skinDescSets.push_back(std::move(descSet));
    }
}