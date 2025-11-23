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