#include "tinyRT/rtScene.hpp"

using namespace tinyRT;

void Scene::init(const SceneRes& res) noexcept {
    res_ = res;

    // Create root node
    Node rootNode;
    rootNode.name = "Root";

    root_ = nodes_.emplace(std::move(rootNode));

    // Initialize mesh machines
    meshStatic3D_.init(res.dvk, &fsr().view<tinyMesh>());
}

void Scene::NWrap::set(Scene* scene, tinyHandle h) noexcept {
    scene_ = scene;
    handle_ = h;
    node_ = get(h);
}

// ---------------------------------------------------------------
// Name APIs
// ---------------------------------------------------------------

std::string& Scene::NWrap::name() noexcept {
    static std::string empty;
    return node_ ? node_->name : empty;
}

const char* Scene::NWrap::cname() const noexcept {
    return node_ ? node_->name.c_str() : "<Invalid>";
}

void Scene::NWrap::rename(const std::string& newName) noexcept {
    if (node_) node_->name = newName;
}

// ---------------------------------------------------------------
// Hierarchy APIs
// ---------------------------------------------------------------

const std::vector<tinyHandle>& Scene::NWrap::children() const noexcept {
    static const std::vector<tinyHandle> empty;
    return node_ ? node_->children : empty;
}
size_t Scene::NWrap::childrenCount() const noexcept {
    return node_ ? node_->children.size() : 0;
}
tinyHandle Scene::NWrap::addChild(const std::string& name) noexcept {
    return scene_ ? scene_->nAdd(name, handle_) : tinyHandle();
}


tinyHandle Scene::NWrap::parent() const noexcept {
    return node_ ? node_->parent : tinyHandle();
}
bool Scene::NWrap::setParent(tinyHandle newParent) noexcept {
    return scene_ && scene_->nReparent(handle_, newParent);
}

void Scene::NWrap::erase(tinyHandle child, bool recursive, size_t* count) noexcept {
    if (scene_) scene_->nErase(child, recursive, count);
}

// ---------------------------------------------------------------
// Actual scene APIs
// ---------------------------------------------------------------

std::vector<tinyHandle> Scene::nQueue(tinyHandle start) noexcept {
    std::vector<tinyHandle> queue; // A DFS queue

    std::function<void(tinyHandle)> addToQueue = [&](tinyHandle h) {
        Node* node = nodes_.get(h);
        if (!node) return;

        for (tinyHandle child : node->children) {
            addToQueue(child);
        }

        queue.push_back(h);
    };
    addToQueue(start);
    return queue;
}

tinyHandle Scene::nAdd(const std::string& name, tinyHandle parent) noexcept {
    Node* nParent = nodes_.get(parent);
    if (!nParent) nParent = nodes_.get(root_);

    Node newNode;
    newNode.name = name;
    newNode.parent = parent;

    tinyHandle nHandle = nodes_.emplace(std::move(newNode));
    nParent->addChild(nHandle);

    return nHandle;
}

void Scene::nErase(tinyHandle nHandle, bool recursive, size_t* count) noexcept {
    Node* node = nodes_.get(nHandle);
    if (!node) return;

    if (Node* parentNode = nodes_.get(node->parent)) {
        parentNode->rmChild(nHandle);
    }

    if (!recursive) {
        nEraseAllComps(nHandle);

        nodes_.remove(nHandle);
        if (count) (*count) = 1;
        return;
    }

    std::vector<tinyHandle> toErase = nQueue(nHandle);
    for (tinyHandle h : toErase) {
        nEraseAllComps(h);
        nodes_.remove(h);
    }
    if (count) (*count) = toErase.size();
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
        rt_.remove(rtHandle);
    }

    node->comps.clear();
}

// ---------------------------------------------------------------
// Scene stuff and things idk
// ---------------------------------------------------------------

void Scene::update(FrameStart frameStart) noexcept {
    float dt = frameStart.deltaTime;
    uint32_t frame = frameStart.frameIndex;

    glm::mat4 parentMat = glm::mat4(1.0f);

    std::function<void(tinyHandle)> updateNode = [&](tinyHandle nHandle) {
        Node* node = nodes_.get(nHandle);
        if (!node) return;

        // Update components
        for (auto& [_, compHandle] : node->comps) {
            if (rtTRANFM3D* tranfm3D = rt_.get<rtTRANFM3D>(compHandle)) {
                tranfm3D->local.update();
                tranfm3D->world = parentMat * tranfm3D->local.Mat4;
                parentMat = tranfm3D->world;
            }
        }

        // Recurse into children
        for (tinyHandle childHandle : node->children) {
            updateNode(childHandle);
        }
    };
    updateNode(root_);
}