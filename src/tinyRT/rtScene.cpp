#include "tinyRT/rtScene.hpp"

using namespace tinyRT;

void Scene::init(const SceneRes& res) noexcept {
    res_ = res;

    // Create root node
    Node rootNode;
    rootNode.name = "Root";

    root_ = nodes_.emplace(std::move(rootNode));

    // Initialize mesh machines
    meshStatic3D_.init(res.deviceVk, &fsr().view<tinyMesh>());
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
    if (!scene_ || !node_) return tinyHandle();

    Node childNode;
    childNode.name = name;
    childNode.parent = handle_;

    tinyHandle childHandle = scene_->nodes_.emplace(std::move(childNode));
    node_->addChild(childHandle);

    return childHandle;
}


tinyHandle Scene::NWrap::parent() const noexcept {
    return node_ ? node_->parent : tinyHandle();
}
bool Scene::NWrap::setParent(tinyHandle newParent) noexcept {
    if (!scene_ || !node_) return false;

    Node* newParentNode = get(newParent);
    if (!newParentNode) return false;

    // Remove from current parent
    if (Node* currentParentNode = get(node_->parent)) {
        currentParentNode->rmChild(handle_);
    }

    // Set new parent
    node_->parent = newParent;
    newParentNode->addChild(handle_);

    return true;
}

bool Scene::NWrap::rmChild(tinyHandle child) noexcept {
    if (!node_) return false;

    Node* childNode = get(child);
    if (!childNode) return false;

    // Clear all components
    for (auto& [_, rtHandle] : childNode->comps) {
        scene_->rt().remove(rtHandle);
    }

    return node_->rmChild(child);
}