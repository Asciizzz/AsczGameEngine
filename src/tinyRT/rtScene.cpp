#include "tinyRT/rtScene.hpp"
#include "tinyData/tinyCamera.hpp"

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
                tinyMesh* mesh = fsr().get<tinyMesh>(meshRD3D->mesh);
                if (!mesh || !cam.collideAABB(mesh->ABmin(), mesh->ABmax(), currentWorld)) continue;

                // Use the mesh static machine
                draw.submit({ meshRD3D->mesh, currentWorld, glm::vec4(0.0f) });
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