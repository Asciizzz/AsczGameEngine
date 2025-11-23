#pragma once

#include "SceneRes.hpp"

// Components
#include "tinyRT/rtMesh.hpp"
#include "tinyRT/rtTransform.hpp"

namespace tinyRT {

struct Node {
    // Empty ahh container 🥀🙏

    std::string name;

    tinyHandle parent;
    std::vector<tinyHandle> children;

    // Entity data
    std::unordered_map<tinyType::ID, tinyHandle> comps;

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
};

class Scene {
// Default stuff
    SceneRes res_;
    tinyRegistry rt_;

    tinyPool<Node> nodes_;
    tinyHandle root_;

// Machines vroom vroom, Max Verstappen
    MeshStatic3D meshStatic3D_;

// Internal helpers
    [[nodiscard]] inline tinyRegistry& fsr() noexcept { return *res_.fsReg; }
    [[nodiscard]] inline Node* node(tinyHandle nHandle) noexcept { return nodes_.get(nHandle); }

    void nEraseComp(tinyHandle nHandle) noexcept;

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

    // A node wrapper struct for easier manipulation
    struct NWrap {
        void set(Scene* scene, tinyHandle h) noexcept;
        const Node* node() const noexcept { return node_; }

        explicit operator bool() const noexcept { return node_ != nullptr; }
        
        template<typename T>
        bool has() const noexcept {
            if (!scene_ || !node_) return false;
            return node_->comps.find(tinyType::TypeID<T>()) != node_->comps.end();
        }

    // Name APIs
        [[nodiscard]] std::string& name() noexcept;
        [[nodiscard]] const char* cname() const noexcept;
        void rename(const std::string& newName) noexcept;

    // Hierarchy APIs
        const std::vector<tinyHandle>& children() const noexcept;
        size_t childrenCount() const noexcept;
        tinyHandle addChild(const std::string& name = "New Node") noexcept;

        tinyHandle parent() const noexcept;
        bool setParent(tinyHandle newParent) noexcept;

    // Indirect APIs

        void erase(tinyHandle nHandle, bool recursive = true, size_t* count = nullptr) noexcept;

    // Component APIs

        template<typename T>
        T* writeComp() noexcept {
            if (!scene_ || !node_) return nullptr;

            // If already exists, return existing
            auto it = node_->comps.find(tinyType::TypeID<T>());
            if (it != node_->comps.end()) return rt().get<T>(it->second);

            T comp{};
            tinyHandle compHandle = rt().emplace<T>(std::move(comp));
            node_->comps[tinyType::TypeID<T>()] = compHandle;

            return rt().get<T>(compHandle);
        }

    private:
        friend class Scene;
        Scene* scene_ = nullptr;

        tinyHandle handle_;
        Node* node_ = nullptr;

        Node* get(tinyHandle h) noexcept {
            return scene_ ? scene_->nodes_.get(h) : nullptr;
        }

        [[nodiscard]] tinyRegistry& rt() noexcept {
            return scene_->rt_;
        }
    };

// Getters
    [[nodiscard]] tinyRegistry& rt() noexcept { return rt_; }
    [[nodiscard]] const tinyRegistry& rt() const noexcept { return rt_; }

    [[nodiscard]] SceneRes& res() noexcept { return res_; }
    [[nodiscard]] const SceneRes& res() const noexcept { return res_; }

    [[nodiscard]] MeshStatic3D& meshStatic3D() noexcept { return meshStatic3D_; }

// Node APIs
    [[nodiscard]] tinyHandle root() const noexcept { return root_; }

    [[nodiscard]] NWrap nWrap(tinyHandle h) noexcept {
        NWrap wrap;
        wrap.set(this, h);
        return wrap;
    }

    inline std::vector<tinyHandle> nQueue(tinyHandle start) noexcept;
    inline tinyHandle nAdd(const std::string& name = "New Node", tinyHandle parent = tinyHandle()) noexcept;
    inline void nErase(tinyHandle nHandle, bool recursive = true, size_t* count = nullptr) noexcept;
};

} // namespace tinyRT

using rtNode = tinyRT::Node;
using rtScene = tinyRT::Scene;