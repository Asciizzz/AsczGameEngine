#include "tinyRT/rtScene.hpp"

// Components
#include "tinyRT/rtTransform.hpp"
#include "tinyRT/rtMesh.hpp"
#include "tinyRT/rtSkeleton.hpp"

using namespace tinyRT;

Scene::~Scene() noexcept {
}


void Scene::init(const SceneRes& res) noexcept {
    res_ = res;

    // Create root node
    Node rootNode;
    rootNode.name = "Root";

    root_ = nodes_.emplace(std::move(rootNode));
}

// ---------------------------------------------------------------
// Node APIs
// ---------------------------------------------------------------

#include <stdexcept>

std::string& Scene::nName(tinyHandle nHandle) noexcept {
    Node* node = nodes_.get(nHandle);
    return node->name;
}

bool Scene::rootShift() noexcept {
    // Shift the root to the child if root only has one child
    Node* rootNode = nodes_.get(root_);
    if (!rootNode || rootNode->children.size() != 1) return false;

    nErase(root_, false);
    root_ = rootNode->children[0];
    return true;
}

std::vector<tinyHandle> Scene::nQueue(tinyHandle start) noexcept {
    std::vector<tinyHandle> queue; // A DFS queue

    std::function<void(tinyHandle)> addToQueue = [&](tinyHandle h) {
        Node* node = nodes_.get(h);
        if (!node) return;

        queue.push_back(h);

        for (tinyHandle child : node->children) addToQueue(child);
    };
    addToQueue(start);
    return queue;
}

tinyHandle Scene::nAdd(const std::string& name, tinyHandle parent) noexcept {
    parent = parent ? parent : root_;
    Node* nParent = nodes_.get(parent);
    if (!nParent) return tinyHandle();

    Node newNode;
    newNode.name = name;
    newNode.parent = parent;

    tinyHandle nHandle = nodes_.emplace(std::move(newNode));
    nodes_.get(parent)->addChild(nHandle);

    return nHandle;
}

void Scene::nErase(tinyHandle nHandle, bool recursive, size_t* count) noexcept {
    Node* node = nodes_.get(nHandle);
    if (!node) return;

    tinyHandle parentHandle = node->parent;
    Node* parentNode = nodes_.get(parentHandle);
    if (parentNode) parentNode->rmChild(nHandle);

    if (!recursive) {
        nEraseAllComps(nHandle);

        // Reparent children to rescue parent
        for (tinyHandle childHandle : node->children) {
            if (Node* childNode = nodes_.get(childHandle)) {
                childNode->parent = parentHandle;

                if (parentNode) parentNode->addChild(childHandle);
            }
        }

        nodes_.erase(nHandle);
        if (count) (*count) = 1;
        return;
    }

    size_t count_ = 0;
    std::function<void(tinyHandle)> eraseRec = [&](tinyHandle h) {
        Node* n = nodes_.get(h);
        if (!n) return;

        std::vector<tinyHandle> childrenCopy = n->children;

        nEraseAllComps(h);
        nodes_.erase(h);
        count_++;

        for (tinyHandle childHandle : childrenCopy) {
            eraseRec(childHandle);
        }
    };
    eraseRec(nHandle);

    if (count) (*count) = count_;
}

tinyHandle Scene::nReparent(tinyHandle nHandle, tinyHandle nNewParent) noexcept {
    Node* node = nodes_.get(nHandle);
    if (!node) return tinyHandle();

    Node* newParent = nodes_.get(nNewParent);
    if (!newParent) return tinyHandle();

    // Check for cyclic parentage
    tinyHandle checkHandle = nNewParent;
    while (checkHandle) {
        if (checkHandle == nHandle) return tinyHandle(); // Cycle detected

        Node* checkNode = nodes_.get(checkHandle);
        if (!checkNode) break;
        checkHandle = checkNode->parent;
    }

    // Remove from current parent
    if (Node* currentParentNode = nodes_.get(node->parent)) {
        currentParentNode->rmChild(nHandle);
    }

    // Set new parent
    node->parent = nNewParent;
    newParent->addChild(nHandle);

    return nHandle;
}


void Scene::nEraseAllComps(tinyHandle nHandle) noexcept {
    Node* node = nodes_.get(nHandle);
    if (!node) return;

    for (auto& [_, rtHandle] : node->comps) {
        rt_.erase(rtHandle);
    }

    node->comps.clear();
}

// ---------------------------------------------------------------
// Scene stuff and things idk
// ---------------------------------------------------------------

void Scene::update(FrameStart frameStart) noexcept {
    float dt = frameStart.deltaTime;
    uint32_t frame = frameStart.frameIndex;

    // Testing ground
    testRenders.clear();

    const tinyCamera& cam = camera();
    tinyDrawable& draw = drawable();

    draw.startFrame(frame);

    std::function<void(tinyHandle, glm::mat4)> updateNode = [&](tinyHandle nHandle, glm::mat4 parentMat) {
        Node* node = nodes_.get(nHandle);
        if (!node) return;

        glm::mat4 currentWorld = parentMat;

        // Update components
        for (auto& [_, compHandle] : node->comps) {
            if (rtTRANFM3D* tranfm3D = rt_.get<rtTRANFM3D>(compHandle)) {
                tranfm3D->local.update();
                tranfm3D->world = parentMat * tranfm3D->local.Mat4;
                currentWorld = tranfm3D->world;
            }

            if (rtMESHRD3D* meshRD3D = rt_.get<rtMESHRD3D>(compHandle)) {
                // Frustum culling
                const tinyMesh* mesh = fsr().get<tinyMesh>(meshRD3D->meshHandle());

                // Mesh culling
                if (!mesh || !cam.collideAABB(mesh->ABmin(), mesh->ABmax(), currentWorld)) continue;

                const Skeleton3D* skele3D = this->nGetComp<Skeleton3D>(meshRD3D->skeleNodeHandle());

                // Submit draw entries
                for (uint32_t subIdx = 0; subIdx < mesh->submeshes().size(); ++subIdx) {
                    const tinyMesh::Submesh* submesh = mesh->submesh(subIdx);

                    // Submesh culling
                    if (!cam.collideAABB(submesh->ABmin, submesh->ABmax, currentWorld)) continue;
                    draw.submit({
                        meshRD3D->meshHandle(),
                        subIdx,
                        currentWorld,
                        { // skin data
                            meshRD3D->skeleNodeHandle(),
                            skele3D ? &skele3D->skinData() : nullptr
                        },
                        meshRD3D->subMrphWeights(subIdx)
                    });
                }
            }

            if (rtSKELE3D* skel3D = rt_.get<rtSKELE3D>(compHandle)) {
                skel3D->update();
            }
        }

        // Recurse into children
        for (tinyHandle childHandle : node->children) {
            updateNode(childHandle, currentWorld);
        }
    };
    updateNode(root_, glm::mat4(1.0f));

    draw.finalize();
}


tinyHandle Scene::instantiate(tinyHandle sceneHandle, tinyHandle parent) noexcept {
    const Scene* fromScene = fsr().get<Scene>(sceneHandle);
    if (!fromScene) return tinyHandle();

    tinyHandle newRootHandle;

    Node* parentNode = nodes_.get(parent);
    if (!parentNode) parent = root_;

    UnorderedMap<tinyHandle, tinyHandle> from_to;
    auto getToHandle = [&](tinyHandle fromHandle) -> tinyHandle {
        auto it = from_to.find(fromHandle);
        return it != from_to.end() ? it->second : tinyHandle();
    };

    std::function<void(tinyHandle, tinyHandle)> cloneRec = [&](tinyHandle fromHandle, tinyHandle toParent) {
        const Node* fromNode = fromScene->node(fromHandle);
        if (!fromNode) return;

        tinyHandle toHandle = nAdd(fromNode->name, toParent);
        Node* toNode = nodes_.get(toHandle);
        from_to[fromHandle] = toHandle;

        // Find new root
        if (fromHandle == fromScene->root()) newRootHandle = toHandle;

        // Establish parent-child relationship
        toNode->parent = toParent;
        if (Node* toParentNode = node(toParent)) toParentNode->addChild(toHandle);

        // Clone components
        if (const rtTRANFM3D* fromTrans = fromScene->nGetComp<rtTRANFM3D>(fromHandle)) {
            rtTRANFM3D* toTrans = nWriteComp<rtTRANFM3D>(toHandle);
            toTrans->local = fromTrans->local;
        }

        if (const rtMESHRD3D* fromMeshRD = fromScene->nGetComp<rtMESHRD3D>(fromHandle)) {
            rtMESHRD3D* toMeshRD = nWriteComp<rtMESHRD3D>(toHandle);
            toMeshRD->
                assignMesh(fromMeshRD->meshHandle(), fromMeshRD->morphs()).
                assignSkeleNode(getToHandle(fromMeshRD->skeleNodeHandle()));
        }

        if (const rtSKELE3D* fromSkele3D = fromScene->nGetComp<rtSKELE3D>(fromHandle)) {
            rtSKELE3D* toSkele3D = nWriteComp<rtSKELE3D>(toHandle);
            toSkele3D->copy(fromSkele3D);
        }

        // Recurse into children
        for (tinyHandle fromChildHandle : fromNode->children) {
            cloneRec(fromChildHandle, toHandle);
        }
    };

    cloneRec(fromScene->root(), parent);

    return newRootHandle;
}