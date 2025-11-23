#pragma once

#include "SceneRes.hpp"

#include "tinyRT/rtMesh.hpp"

namespace tinyRT {

struct Entity {
    std::unordered_map<tinyType::ID, tinyHandle> comps;
};

struct Node {
    // Empty ahh container 🥀🙏

    std::string name;

    tinyHandle parent;
    std::vector<tinyHandle> children;

    Entity entity; // Node is entity itself
};

class Scene {
    SceneRes res_;
    tinyRegistry rt_;

    tinyPool<Node> nodes_;
    tinyHandle root_;

    // Mesh machines vroom vroom
    MeshStatic3D meshStatic3D_;

    inline tinyRegistry& fsr() noexcept { return *res_.fsReg; }

public:
    Scene() noexcept = default;
    void init(const SceneRes& res) noexcept;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    Scene(Scene&&) noexcept = default;
    Scene& operator=(Scene&&) noexcept = default;

// Getters
    [[nodiscard]] tinyRegistry& rt() noexcept { return rt_; }
    [[nodiscard]] const tinyRegistry& rt() const noexcept { return rt_; }

    [[nodiscard]] SceneRes& res() noexcept { return res_; }
    [[nodiscard]] const SceneRes& res() const noexcept { return res_; }
};

} // namespace tinyRT

using rtNode = tinyRT::Node;
using rtScene = tinyRT::Scene;