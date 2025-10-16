#include "TinyEngine/TinyProject.hpp"
#include <imgui.h>
#include <algorithm>

using NTypes = TinyNode::Types;

// A quick function for range validation
template<typename T>
bool isValidIndex(int index, const std::vector<T>& vec) {
    return index >= 0 && index < static_cast<int>(vec.size());
}


TinyProject::TinyProject(const TinyVK::Device* deviceVK) : deviceVK(deviceVK) {
    // registry = MakeUnique<TinyRegistry>();
    tinyFS = MakeUnique<TinyFS>();

    TinyHandle registryHandle = tinyFS->addFolder(".registry");
    tinyFS->setRegistryHandle(registryHandle);

    // Create CoreScene (the active scene with a single root node)
    TinyRScene coreScene;
    coreScene.name = "CoreScene";
    
    // Create root node for the core scene
    TinyRNode rootNode;
    rootNode.name = "Root";
    rootNode.localTransform = glm::mat4(1.0f);
    rootNode.globalTransform = glm::mat4(1.0f);
    
    coreScene.rootNode = coreScene.nodes.insert(std::move(rootNode));
    
    // Add CoreScene to registry and store the handle
    TinyHandle coreSceneHandle = tinyFS->addToRegistry(coreScene).handle;
    activeSceneHandle = coreSceneHandle;

    // Create camera and global UBO manager
    tinyCamera = MakeUnique<TinyCamera>(glm::vec3(0.0f, 0.0f, 0.0f), 45.0f, 0.1f, 100.0f);
    tinyGlobal = MakeUnique<TinyGlobal>(2);
    tinyGlobal->createVkResources(deviceVK);

    // Create default material and texture
    TinyTexture defaultTexture = TinyTexture::createDefaultTexture();
    TinyRTexture defaultRTexture = TinyRTexture(defaultTexture);
    defaultRTexture.create(deviceVK);

    defaultTextureHandle = tinyFS->addToRegistry(defaultRTexture).handle;

    TinyRMaterial defaultMaterial;
    defaultMaterial.setAlbTexIndex(0);
    defaultMaterial.setNrmlTexIndex(0);

    defaultMaterialHandle = tinyFS->addToRegistry(defaultMaterial).handle;
}

TinyHandle TinyProject::addSceneFromModel(const TinyModel& model) {
    // Create a folder for the model
    TinyHandle fnModelFolder = tinyFS->addFolder(model.name);
    TinyHandle fnTexFolder = tinyFS->addFolder(fnModelFolder, "Textures");
    TinyHandle fnMatFolder = tinyFS->addFolder(fnModelFolder, "Materials");
    TinyHandle fnMeshFolder = tinyFS->addFolder(fnModelFolder, "Meshes");
    TinyHandle fnSkeleFolder = tinyFS->addFolder(fnModelFolder, "Skeletons");

    // Note: fnHandle - handle to file node in TinyFS's fnodes
    //       tHandle - handle to the actual data in the registry (infused with Type info for TinyFS usage)

    // Import textures to registry
    std::vector<TinyHandle> glbTexRHandle;
    for (const auto& texture : model.textures) {
        TinyRTexture rTexture = TinyRTexture();
        rTexture.create(deviceVK);

        // TinyHandle handle = registry->add(textureData).handle;
        TinyHandle fnHandle = tinyFS->addFile(fnTexFolder, texture.name, std::move(&rTexture));
        TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

        glbTexRHandle.push_back(tHandle.handle);
    }

    // Import materials to registry with remapped texture references
    std::vector<TinyHandle> glmMatRHandle;
    for (const auto& material : model.materials) {
        TinyRMaterial correctMat;
        correctMat.name = material.name; // Copy material name

        // Remap the material's texture indices
        uint32_t localAlbIndex = material.localAlbTexture;
        bool localAlbValid = localAlbIndex >= 0 && localAlbIndex < static_cast<int>(glbTexRHandle.size());
        correctMat.setAlbTexIndex(localAlbValid ? glbTexRHandle[localAlbIndex].index : 0);

        uint32_t localNrmlIndex = material.localNrmlTexture;
        bool localNrmlValid = localNrmlIndex >= 0 && localNrmlIndex < static_cast<int>(glbTexRHandle.size());
        correctMat.setNrmlTexIndex(localNrmlValid ? glbTexRHandle[localNrmlIndex].index : 0);

        // TinyHandle handle = registry->add(correctMat).handle;
        TinyHandle fnHandle = tinyFS->addFile(fnMatFolder, material.name, &correctMat);
        TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

        glmMatRHandle.push_back(tHandle.handle);
    }

    // Import meshes to registry with remapped material references
    std::vector<TinyHandle> glbMeshRHandle;
    for (const auto& mesh : model.meshes) {
        TinyRMesh rMesh = TinyRMesh(mesh);
        rMesh.create(deviceVK);

        // Remap submeshes' material indices
        std::vector<TinySubmesh> remappedSubmeshes = mesh.submeshes;
        for (auto& submesh : remappedSubmeshes) {
            bool valid = isValidIndex(submesh.material.index, glmMatRHandle);
            submesh.material = valid ? glmMatRHandle[submesh.material.index] : TinyHandle();
        }

        rMesh.setSubmeshes(remappedSubmeshes);

        TinyHandle fnHandle = tinyFS->addFile(fnMeshFolder, mesh.name, std::move(&rMesh));
        TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

        glbMeshRHandle.push_back(tHandle.handle);
    }

    // Import skeletons to registry
    std::vector<TinyHandle> glbSkeleRHandle;
    for (const auto& skeleton : model.skeletons) {
        TinyRSkeleton rSkeleton = TinyRSkeleton(skeleton);

        // TinyHandle handle = registry->add(rSkeleton).handle;
        TinyHandle fnHandle = tinyFS->addFile(fnSkeleFolder, "Skeleton", &rSkeleton);
        TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

        glbSkeleRHandle.push_back(tHandle.handle);
    }

    // Create scene with nodes - preserve hierarchy but remap resource references
    TinyRScene scene;
    scene.name = model.name;

    // Since we are mostly loading this model in a vacuum
    // The pool structure initially matches the model's node array 1:1

    if (!model.nodes.empty()) {
        // First node is root (should be correct according to TinyLoader implementation)
        scene.rootNode = TinyHandle(0, 1);
    }

    for (const auto& node : model.nodes) {
        TinyRNode rtNode = node; // Copy node data

        // Remap MeshRender component's mesh reference
        if (rtNode.hasType(NTypes::MeshRender)) {
            auto* meshRender = rtNode.get<TinyRNode::MeshRender>();
            if (meshRender) meshRender->meshHandle = glbMeshRHandle[meshRender->meshHandle.index];
        }

        // Remap Skeleton component's registry reference
        if (rtNode.hasType(NTypes::Skeleton)) {
            auto* skeleton = rtNode.get<TinyRNode::Skeleton>();
            if (skeleton) {
                skeleton->skeleHandle = glbSkeleRHandle[skeleton->skeleHandle.index];

                // Construct the final bone transforms array (set to identity, resolve through skeleton later)
                const auto& skeleData = registryRef().get<TinyRSkeleton>(skeleton->skeleHandle);
                if (skeleData) skeleton->boneTransformsFinal.resize(skeleData->bones.size(), glm::mat4(1.0f));
            }
        }

        scene.nodes.insert(std::move(rtNode));
    }

    // Add scene to registry and return the handle
    TinyHandle fnHandle = tinyFS->addFile(fnModelFolder, scene.name, &scene);
    TypeHandle tHandle = tinyFS->getTHandle(fnHandle);

    return tHandle.handle;
}



void TinyProject::addSceneInstance(TinyHandle sceneHandle, TinyHandle parentNode) {
    const auto& registry = registryRef();
    const TinyRScene* sourceScene = registry.get<TinyRScene>(sceneHandle);
    TinyRScene* activeScene = getActiveScene();
    
    if (!sourceScene || !activeScene) return;

    // Use root node if no valid parent provided
    TinyHandle actualParent = parentNode.valid() ? parentNode : activeScene->rootNode;
    
    // Use the scene's addSceneToNode method to merge the source scene into active scene
    activeScene->addSceneToNode(*sourceScene, actualParent);
    
    // Update transforms for the entire active scene
    activeScene->updateGlbTransform();
}




void TinyProject::runPlayground(float dTime) {
    return;
}







// Scene deletion can be handled through registry cleanup
// Runtime nodes are managed through the rtNodes vector directly