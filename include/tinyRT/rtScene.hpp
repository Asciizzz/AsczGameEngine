#pragma once


#include "tinyRegistry.hpp"
#include "tinyCamera.hpp"
#include "tinyDrawable.hpp"

namespace tinyRT {

struct SceneRes {
    uint32_t maxFramesInFlight = 0; // If you messed this up the app just straight up jump off a cliff

    tinyRegistry* fsr = nullptr; // For stuffs and things
    const tinyVk::Device* dvk = nullptr;   // For GPU resource creation
    tinyCamera* camera = nullptr;    // For global UBOs
    tinyDrawable* drawable = nullptr;

// File system helper

    template<typename T> tinyPool<T>& fsView() { return fsr->view<T>(); }
    template<typename T> const tinyPool<T>& fsView() const { return fsr->view<T>(); }

    template<typename T> T* fsGet(tinyHandle handle) { return fsr->get<T>(handle); }
    template<typename T> const T* fsGet(tinyHandle handle) const { return fsr->get<T>(handle); }
};

struct Node {
    // Empty ahh container 🥀🙏
    Node() noexcept = default; // Make non-trivial

    std::string name;
    const char* cname() const noexcept { return name.c_str(); }

    tinyHandle parent;
    std::vector<tinyHandle> children;

    // Entity data
    std::map<tinyType::ID, tinyHandle> comps;

// Some helpers

    size_t childrenCount() const noexcept {
        return children.size();
    }

    int whereChild(tinyHandle child) const noexcept {
        for (size_t i = 0; i < children.size(); ++i) {
            if (children[i] == child) return static_cast<int>(i);
        }
        return -1;
    }

    int addChild(tinyHandle child) noexcept {
        int idx = whereChild(child);
        if (idx != -1) return idx;
        children.push_back(child);
        return static_cast<int>(children.size() - 1);
    }

    bool rmChild(tinyHandle child) noexcept {
        int idx = whereChild(child);
        if (idx == -1) return false;
        children.erase(children.begin() + idx);
        return true;
    }

    template<typename T>
    bool has() const noexcept {
        return comps.find(tinyType::TypeID<T>()) != comps.end();
    }

    template<typename T>
    tinyHandle get() const noexcept {
        auto it = comps.find(tinyType::TypeID<T>());
        return it != comps.end() ? it->second : tinyHandle();
    }

    template<typename T>
    void erase() noexcept {
        auto it = comps.find(tinyType::TypeID<T>());
        if (it != comps.end()) comps.erase(it);
    }

    template<typename T>
    void add(tinyHandle h) noexcept {
        comps[tinyType::TypeID<T>()] = h;
    }
};

class Scene {
// Default stuff
    SceneRes res_;
    tinyRegistry rt_;

    tinyPool<Node> nodes_;
    tinyHandle root_;

// Internal helpers
    [[nodiscard]] inline tinyRegistry& fsr() noexcept { return *res_.fsr; }
    [[nodiscard]] inline tinyCamera& camera() noexcept { return *res_.camera; }

public:
    Scene() noexcept = default;
    void init(const SceneRes& res) noexcept;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    Scene(Scene&&) noexcept = default;
    Scene& operator=(Scene&&) noexcept = default;

// Scene specific structs
    struct FrameStart {
        uint32_t frameIndex = 0;
        float deltaTime = 0.0f;
    };

// Getters
    [[nodiscard]] tinyRegistry& rt() noexcept { return rt_; }
    [[nodiscard]] const tinyRegistry& rt() const noexcept { return rt_; }

    [[nodiscard]] SceneRes& res() noexcept { return res_; }
    [[nodiscard]] const SceneRes& res() const noexcept { return res_; }

    [[nodiscard]] tinyDrawable& drawable() noexcept { return *res_.drawable; }
    [[nodiscard]] const tinyDrawable& drawable() const noexcept { return *res_.drawable; }

// Node APIs
    std::string& nName(tinyHandle nHandle) noexcept;

    [[nodiscard]] tinyHandle rootHandle() const noexcept { return root_; }
    bool rootShift() noexcept;

    [[nodiscard]] Node* root() noexcept { return nodes_.get(root_); }
    [[nodiscard]] const Node* root() const noexcept { return nodes_.get(root_); }

    [[nodiscard]] Node* node(tinyHandle nHandle) noexcept { return nodes_.get(nHandle); }
    [[nodiscard]] const Node* node(tinyHandle nHandle) const noexcept { return nodes_.get(nHandle); }

    [[nodiscard]] std::vector<tinyHandle> nQueue(tinyHandle start) noexcept;
    tinyHandle nAdd(const std::string& name = "New Node", tinyHandle parent = tinyHandle()) noexcept;
    void nErase(tinyHandle nHandle, bool recursive = true, size_t* count = nullptr) noexcept;
    tinyHandle nReparent(tinyHandle nHandle, tinyHandle nNewParent) noexcept;

    template<typename T>
    T* nGetComp(tinyHandle nHandle) noexcept {
        const Node* node = nodes_.get(nHandle);
        if (!node || !node->has<T>()) return nullptr;

        return rt_.get<T>(node->get<T>());
    }

    template<typename T>
    const T* nGetComp(tinyHandle nHandle) const noexcept {
        const Node* node = nodes_.get(nHandle);
        if (!node || !node->has<T>()) return nullptr;

        return rt_.get<T>(node->get<T>());
    }

    template<typename T>
    tinyHandle nAddComp(tinyHandle nHandle) noexcept { // Generic component addition (without data)
        Node* node = nodes_.get(nHandle);
        if (!node || node->has<T>()) return tinyHandle();

        tinyHandle compHandle = rt_.emplace<T>();
        node->add<T>(compHandle);

        return compHandle;
    }

    template<typename T>
    T* nWriteComp(tinyHandle nHandle) noexcept { // Automatic resolver
        tinyHandle compHandle = nAddComp<T>(nHandle);
        return rt_.get<T>(compHandle);
    }

    template<typename T>
    void nEraseComp(tinyHandle nHandle) noexcept {
        Node* node = nodes_.get(nHandle);
        if (!node || !node->has<T>()) return;

        rt_.erase(node->get<T>());
        node->erase<T>();
    }

    void nEraseAllComps(tinyHandle nHandle) noexcept;

// Special scene methods
    void cleanse() noexcept {} // Rewire pool into clean DFS order (to be implemented)

// Scene stuff and things idk
    void update(FrameStart frameStart) noexcept;

    tinyHandle instantiate(tinyHandle sceneHandle, tinyHandle parent = tinyHandle()) noexcept;


// Testing ground

    struct TestRender {
        glm::mat4 model;
        tinyHandle mesh;
    };
    std::vector<TestRender> testRenders;
};

} // namespace tinyRT

using rtNode = tinyRT::Node;
using rtScene = tinyRT::Scene;
using rtSceneRes = tinyRT::SceneRes;