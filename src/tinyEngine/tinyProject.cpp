#include "tinyEngine/tinyProject.hpp"
#include "tinyEngine/tinyLoader.hpp"

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

tinyProject::tinyProject(const tinyVk::Device* dvk) : dvk_(dvk) {
    // Create camera and global UBO manager
    camera_ = MakeUnique<tinyCamera>(glm::vec3(0.0f, 0.0f, 0.0f), 45.0f, 0.5f, 1000.0f);
    global_ = MakeUnique<tinyGlobal>(2);
    global_->vkCreate(dvk_);

    setupResources();
}

tinyProject::~tinyProject() {
    global_.reset();
    camera_.reset();
    fs_.reset();
}



#include "tinyRT/rtTransform.hpp"
#include "tinyRT/rtMesh.hpp"
#include "tinyRT/rtSkeleton.hpp"

tinyHandle tinyProject::addModel(tinyModel& model, tinyHandle parentFolder) {
    parentFolder = parentFolder ? parentFolder : fs_->rootHandle();

    tinyHandle fnModelFolder = fs_->createFolder(model.name, parentFolder);

    // Import textures to registry
    std::vector<tinyHandle> glbTexRHandle;
    if (!model.textures.empty()) {
        tinyHandle fnTexFolder = fs_->createFolder("Textures", fnModelFolder);
        for (auto& mTexture : model.textures) {
            mTexture.texture.vkCreate(dvk_);

            tinyHandle fnHandle = fs_->createFile(mTexture.name, std::move(mTexture.texture), fnTexFolder);
            tinyHandle dataHandle = fs_->dataHandle(fnHandle);
            glbTexRHandle.push_back(dataHandle);
        }
    }

    std::vector<tinyHandle> glmMatRHandle;
    if (!model.materials.empty()) {
        tinyHandle fnMatFolder = fs_->createFolder("Materials", fnModelFolder);

        for (const auto& mMaterial : model.materials) {
            tinyMaterial material;

            // Set material base color from model
            material.baseColor = mMaterial.baseColor;

            tinyHandle albHandle = linkHandle(mMaterial.albIndx, glbTexRHandle);
            material.albTexture = albHandle;

            tinyHandle nrmlHandle = linkHandle(mMaterial.nrmlIndx, glbTexRHandle);
            material.nrmlTexture = nrmlHandle;

            tinyHandle emissHandle = linkHandle(mMaterial.emisIndx, glbTexRHandle);
            material.emissTexture = emissHandle;

            tinyHandle fnHandle = fs_->createFile(mMaterial.name, std::move(material), fnMatFolder);
            tinyHandle dataHandle = fs_->dataHandle(fnHandle);
            glmMatRHandle.push_back(dataHandle);
        }
    }

    std::vector<tinyHandle> glbMeshRHandle;
    if (!model.meshes.empty()) {
        tinyHandle fnMeshFolder = fs_->createFolder("Meshes", fnModelFolder);

        for (auto& mMesh : model.meshes) {
            mMesh.mesh.vkCreate(dvk_, drawable_->vrtxExtLayout(), drawable_->vrtxExtPool());

            for (auto& submesh : mMesh.mesh.submeshes()) {
                if (validIndex(submesh.material.index, glmMatRHandle)) {
                    submesh.material = glmMatRHandle[submesh.material.index];
                }
            }

            tinyHandle fnHandle = fs_->createFile(mMesh.name, std::move(mMesh.mesh), fnMeshFolder);
            tinyHandle dataHandle = fs_->dataHandle(fnHandle);
            glbMeshRHandle.push_back(dataHandle);
        }
    }

    // Import skeletons to registry
    std::vector<tinyHandle> glbSkeleRHandle;
    if (!model.skeletons.empty()) {
        tinyHandle fnSkeleFolder = fs_->createFolder("Skeletons", fnModelFolder);
        for (auto& mSkeleton : model.skeletons) {

            tinyHandle fnHandle = fs_->createFile(mSkeleton.name, std::move(mSkeleton.skeleton), fnSkeleFolder);
            tinyHandle dataHandle = fs_->dataHandle(fnHandle);
            glbSkeleRHandle.push_back(dataHandle);
        }
    }

    // If scene has no nodes, return early
    if (model.nodes.empty()) return tinyHandle();

    // Create scene with nodes - preserve hierarchy but remap resource references
    rtScene scene;
    scene.init(sharedRes_);

    // Recursively add nodes
    std::unordered_map<int, tinyHandle> nodeMap; // Original index to new handle

    std::function<void(int, tinyHandle)> addNodeRecursive = [&](int nodeIndex, tinyHandle parentHandle) {
        const tinyModel::Node& ogNode = model.nodes[nodeIndex];

        // Ignore the root node for naming purposes
        tinyHandle nodeHandle = nodeIndex ? scene.nAdd(ogNode.name, parentHandle) : scene.rootHandle();
        nodeMap[nodeIndex] = nodeHandle;

        if (ogNode.hasTRFM3D()) {
            rtTRANFM3D* trfm = scene.nWriteComp<rtTRANFM3D>(nodeHandle);
            trfm->local = ogNode.TRFM3D;
        }

        if (ogNode.hasMESHR()) {
            rtMESHRD3D* meshrd = scene.nWriteComp<rtMESHRD3D>(nodeHandle);

            tinyHandle meshHandle = linkHandle(ogNode.MESHRD_meshIndx, glbMeshRHandle);
            if (const tinyMesh* meshPtr = fs().rGet<tinyMesh>(meshHandle)) {
                tinyHandle skeleNodeHandle =
                    nodeMap.count(ogNode.MESHRD_skeleNodeIndx) ?
                    nodeMap[ogNode.MESHRD_skeleNodeIndx] : tinyHandle();

                meshrd->
                    assignMesh(meshHandle, meshPtr).
                    assignSkeleNode(skeleNodeHandle);
            }
        }

        if (ogNode.hasSKEL3D()) {
            rtSKELE3D* skele3D = scene.nWriteComp<rtSKELE3D>(nodeHandle);
            skele3D->init(
                &r().view<tinySkeleton>(),
                linkHandle(ogNode.SKEL3D_skeleIndx, glbSkeleRHandle)
            );
        }

        for (int childIndex : ogNode.children) {
            addNodeRecursive(childIndex, nodeHandle);
        }
    };
    addNodeRecursive(0, tinyHandle());

    // Rename the root node to the model's name
    scene.root()->name = model.name;

    // Add scene to registry
    tinyHandle fnHandle = fs_->createFile(model.name, std::move(scene), fnModelFolder);
    return fs_->dataHandle(fnHandle); // Return the scene's registry handle
}

void tinyProject::addSceneInstance(tinyHandle fromHandle, tinyHandle toHandle, tinyHandle parentHandle) {
    if (fromHandle == toHandle) return; // Prevent self-copy

    if (rtScene* toScene = r().get<rtScene>(toHandle)) {
        toScene->instantiate(fromHandle, parentHandle);
    }
}

// ------------------- Filesystem Setup -------------------

void tinyProject::setupResources() {
    fs_ = MakeUnique<tinyFS>();
    drawable_ = MakeUnique<tinyDrawable>();
    drawable_->init({
        2, // max frames in flight 
        &fs_->r(),
        dvk_,
    });

    // Setup shared scene requirements
    sharedRes_.maxFramesInFlight = drawable_->maxFramesInFlight();
    sharedRes_.fsr = &fs_->r();
    sharedRes_.dvk = dvk_;
    sharedRes_.camera = camera_.get();
    sharedRes_.drawable = drawable_.get();

// -------------------- Setup FS extensions --------------------

    // ------------------ Standard files ------------------

    tinyFS::TypeInfo* ascn = fs_->typeInfo<rtScene>();
    ascn->ext = "ascn"; ascn->rmOrder = 10;
    ascn->color[0] = 102; ascn->color[1] = 255; ascn->color[2] = 102;

    tinyFS::TypeInfo* amat = fs_->typeInfo<tinyMaterial>();
    amat->ext = "amat";
    amat->color[0] = 255; amat->color[1] = 102; amat->color[2] = 255;

    tinyFS::TypeInfo* atex = fs_->typeInfo<tinyTexture>();
    atex->ext = "atex"; atex->rmOrder = 1;
    atex->color[0] = 102; atex->color[1] = 102; atex->color[2] = 255;

    atex->onCreate = [&](tinyHandle fileHandle, tinyFS& fs, void* userData) {
        tinyHandle dataHandle = fs.dataHandle(fileHandle);
        tinyTexture* texture = fs.rGet<tinyTexture>(dataHandle);
        if (!texture) return;

        // Add to tinyDrawable                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       
        if (tinyDrawable* drawable = drawable_.get()) {
            texture->setDrawableIndex(drawable->addTexture(dataHandle));
        }
    };

    atex->onDelete = [&](tinyHandle fileHandle, tinyFS& fs, void* userData) {
        tinyHandle dataHandle = fs.dataHandle(fileHandle);

        // Remove from tinyDrawable
        if (tinyDrawable* drawable = drawable_.get()) {
            drawable->removeTexture(dataHandle);
        }
    };

    tinyFS::TypeInfo* amsh = fs_->typeInfo<tinyMesh>();
    amsh->ext = "amsh";
    amsh->color[0] = 255; amsh->color[1] = 255; amsh->color[2] = 102;

    tinyFS::TypeInfo* askl = fs_->typeInfo<tinySkeleton>();
    askl->ext = "askl";
    askl->color[0] = 102; askl->color[1] = 255; askl->color[2] = 255;

    tinyFS::TypeInfo* ascr = fs_->typeInfo<tinyScript>();
    ascr->ext = "ascr"; ascr->rmOrder = 2;
    ascr->color[0] = 204; ascr->color[1] = 204; ascr->color[2] = 51;

    // ------------------- Special "files" -------------------

    // Resources that lives in the registry but not registered (pun intended)
    // as files in the filesystem

    tinyFS::TypeInfo* descPool = fs_->typeInfo<tinyVk::DescPool>();
    descPool->rmOrder = UINT8_MAX;

    tinyFS::TypeInfo* descLayout = fs_->typeInfo<tinyVk::DescSLayout>();
    descLayout->rmOrder = UINT8_MAX;

    tinyFS::TypeInfo* descSet = fs_->typeInfo<tinyVk::DescSet>();
    descSet->rmOrder = UINT8_MAX - 1; // Delete before desc pools

    tinyFS::TypeInfo* dataBuffer = fs_->typeInfo<tinyVk::DataBuffer>();
    dataBuffer->rmOrder = UINT8_MAX - 2; // Delete before desc sets

    // ------------------ Other useful files ------------------
    tinyFS::TypeInfo* atxt = fs_->typeInfo<tinyText>();
    atxt->ext = "txt";
    atxt->color[0] = 153; atxt->color[1] = 153; atxt->color[2] = 153;

// ------------------------ Default resources ------------------------

    rtScene mainScene;
    mainScene.init(sharedRes_);

    tinyHandle mainSceneFileHandle = fs_->createFile("Main Scene", std::move(mainScene));

    mainSceneHandle = fs_->dataHandle(mainSceneFileHandle);
}

// ------------------ Pending Removals ------------------

void tinyProject::fRemove(tinyHandle fileHandle) {
    const tinyNodeFS* node = fs_->fNode(fileHandle);
    if (!node) return;

    std::vector<tinyHandle> rmQueue = fs_->fQueue(fileHandle);
    std::sort(rmQueue.begin(), rmQueue.end(), [&](tinyHandle a, tinyHandle b) {
        const tinyFS::TypeInfo* tInfoA = fs().typeInfo(a);
        const tinyFS::TypeInfo* tInfoB = fs().typeInfo(b);
        uint8_t orderA = tInfoA ? tInfoA->rmOrder : 255;
        uint8_t orderB = tInfoB ? tInfoB->rmOrder : 255;

        return orderA < orderB;
    });

    for (tinyHandle h : rmQueue) {
        tinyHandle dHandle = fs_->dataHandle(h);

        if (dHandle.is<tinyMesh>() ||
            dHandle.is<tinyMaterial>() ||
            dHandle.is<tinyTexture>() ||
            dHandle.is<rtScene>())
        {
            deferredRms_[DeferRmType::Vulkan].push_back(h);
        } else {
            fs_->rmRaw(h); // Remove this node immediately
        }
    }
}

void tinyProject::execDeferredRms(DeferRmType type) {
    auto it = deferredRms_.find(type);
    if (it == deferredRms_.end()) return;

    for (tinyHandle h : it->second) fs_->rmRaw(h);

    it->second.clear();
}

bool tinyProject::hasDeferredRms(DeferRmType type) const {
    auto it = deferredRms_.find(type);
    return it != deferredRms_.end() && !it->second.empty();
}