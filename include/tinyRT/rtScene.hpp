#pragma once


#include "ascReg.hpp"
#include "tinyCamera.hpp"
#include "tinyDrawable.hpp"

// Components
#include "tinyRT/rtTransform.hpp"
#include "tinyRT/rtMesh.hpp"
#include "tinyRT/rtSkeleton.hpp"
#include "tinyRT/rtScript.hpp"

namespace tinyRT {

struct SceneRes {
    uint32_t maxFramesInFlight = 0; // If you messed this up the app just straight up jump off a cliff

    Asc::Reg* fsr = nullptr; // For stuffs and things
    const tinyVk::Device* dvk = nullptr;   // For GPU resource creation
    tinyCamera* camera = nullptr;    // For global UBOs
    tinyDrawable* drawable = nullptr;

// File system helper

    template<typename T> Asc::Pool<T>& fsView() { return fsr->view<T>(); }
    template<typename T> const Asc::Pool<T>& fsView() const { return fsr->view<T>(); }

    template<typename T> T* fsGet(Asc::Handle handle) { return fsr->get<T>(handle); }
    template<typename T> const T* fsGet(Asc::Handle handle) const { return fsr->get<T>(handle); }
};

struct Node {
    // Empty ahh container 🥀🙏
    Node() noexcept = default; // Make non-trivial

    std::string name;
    const char* cname() const noexcept { return name.c_str(); }

    Asc::Handle parent;
    std::vector<Asc::Handle> children;

    // Entity data
    std::map<Asc::Type::ID, Asc::Handle> comps;

// Some helpers

    size_t childrenCount() const noexcept {
        return children.size();
    }

    int whereChild(Asc::Handle child) const noexcept {
        for (size_t i = 0; i < children.size(); ++i) {
            if (children[i] == child) return static_cast<int>(i);
        }
        return -1;
    }

    int addChild(Asc::Handle child) noexcept {
        int idx = whereChild(child);
        if (idx != -1) return idx;
        children.push_back(child);
        return static_cast<int>(children.size() - 1);
    }

    bool rmChild(Asc::Handle child) noexcept {
        int idx = whereChild(child);
        if (idx == -1) return false;
        children.erase(children.begin() + idx);
        return true;
    }

    template<typename T>
    bool has() const noexcept {
        return comps.find(Asc::Type::TypeID<T>()) != comps.end();
    }

    template<typename T>
    Asc::Handle get() const noexcept {
        auto it = comps.find(Asc::Type::TypeID<T>());
        return it != comps.end() ? it->second : Asc::Handle();
    }

    template<typename T>
    void erase() noexcept {
        auto it = comps.find(Asc::Type::TypeID<T>());
        if (it != comps.end()) comps.erase(it);
    }

    template<typename T>
    void add(Asc::Handle h) noexcept {
        comps[Asc::Type::TypeID<T>()] = h;
    }
};

class Scene {
// Default stuff
    SceneRes res_;
    Asc::Reg rt_;

    Asc::Pool<Node> nodes_;
    Asc::Handle root_;

// Internal helpers
    [[nodiscard]] inline Asc::Reg& fsr() noexcept { return *res_.fsr; }
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
    [[nodiscard]] Asc::Reg& rt() noexcept { return rt_; }
    [[nodiscard]] const Asc::Reg& rt() const noexcept { return rt_; }

    [[nodiscard]] SceneRes& res() noexcept { return res_; }
    [[nodiscard]] const SceneRes& res() const noexcept { return res_; }

    [[nodiscard]] tinyDrawable& drawable() noexcept { return *res_.drawable; }
    [[nodiscard]] const tinyDrawable& drawable() const noexcept { return *res_.drawable; }

// Node APIs
    std::string& nName(Asc::Handle nHandle) noexcept;

    [[nodiscard]] Asc::Handle rootHandle() const noexcept { return root_; }
    bool rootShift() noexcept;

    [[nodiscard]] Node* root() noexcept { return nodes_.get(root_); }
    [[nodiscard]] const Node* root() const noexcept { return nodes_.get(root_); }

    [[nodiscard]] Node* node(Asc::Handle nHandle) noexcept { return nodes_.get(nHandle); }
    [[nodiscard]] const Node* node(Asc::Handle nHandle) const noexcept { return nodes_.get(nHandle); }

    [[nodiscard]] std::vector<Asc::Handle> nQueue(Asc::Handle start) noexcept;
    Asc::Handle nAdd(const std::string& name = "New Node", Asc::Handle parent = Asc::Handle()) noexcept;
    void nErase(Asc::Handle nHandle, bool recursive = true, size_t* count = nullptr) noexcept;
    Asc::Handle nReparent(Asc::Handle nHandle, Asc::Handle nNewParent) noexcept;

    template<typename T>
    T* nGetComp(Asc::Handle nHandle) noexcept {
        const Node* node = nodes_.get(nHandle);
        if (!node || !node->has<T>()) return nullptr;

        return rt_.get<T>(node->get<T>());
    }

    template<typename T>
    const T* nGetComp(Asc::Handle nHandle) const noexcept {
        const Node* node = nodes_.get(nHandle);
        if (!node || !node->has<T>()) return nullptr;

        return rt_.get<T>(node->get<T>());
    }

    template<typename T>
    Asc::Handle nAddComp(Asc::Handle nHandle) noexcept { // Generic component addition (without data)
        Node* node = nodes_.get(nHandle);
        if (!node || node->has<T>()) return Asc::Handle();

        Asc::Handle compHandle = rt_.emplace<T>();
        node->add<T>(compHandle);

        return compHandle;
    }

    template<typename T>
    T* nWriteComp(Asc::Handle nHandle) noexcept { // Automatic resolver
        Asc::Handle compHandle = nAddComp<T>(nHandle);
        return rt_.get<T>(compHandle);
    }

    template<typename T>
    void nEraseComp(Asc::Handle nHandle) noexcept {
        Node* node = nodes_.get(nHandle);
        if (!node || !node->has<T>()) return;

        rt_.erase(node->get<T>());
        node->erase<T>();
    }

    void nEraseAllComps(Asc::Handle nHandle) noexcept;

// Special scene methods
    void cleanse() noexcept {} // Rewire pool into clean DFS order (to be implemented)

// Scene stuff and things idk
    void update(FrameStart frameStart) noexcept;

    Asc::Handle instantiate(Asc::Handle sceneHandle, Asc::Handle parent = Asc::Handle()) noexcept;


// Testing ground

    struct TestRender {
        glm::mat4 model;
        Asc::Handle mesh;
    };
    std::vector<TestRender> testRenders;
};

} // namespace tinyRT

using rtNode = tinyRT::Node;
using rtScene = tinyRT::Scene;
using rtSceneRes = tinyRT::SceneRes;