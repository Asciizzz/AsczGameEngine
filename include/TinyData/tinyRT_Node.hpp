#pragma once

#include "tinyExt/tinyHandle.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <tuple>

namespace tinyRT {

struct Node {
    std::string name = "Node";
    Node(const std::string& nodeName = "Node") : name(nodeName) {}

    enum class Types : uint32_t {
        TRFM3D = 1 << 0, // Transform3D
        MESHRD = 1 << 1, // MeshRender3D
        SKEL3D = 1 << 2, // Skeleton3D
        BONE3D = 1 << 3, // BoneAttach3D
        ANIM3D = 1 << 4, // Animation3D
        SCRIPT = 1 << 5  // Script
    };

    tinyHandle parentHandle;
    std::vector<tinyHandle> childrenHandles;

    void setParent(tinyHandle newParent) {
        parentHandle = newParent;
    }

    void addChild(tinyHandle childHandle) {
        childrenHandles.push_back(childHandle);
    }

    void removeChild(tinyHandle childHandle) {
        childrenHandles.erase(std::remove(childrenHandles.begin(), childrenHandles.end(), childHandle), childrenHandles.end());
    }

// ------------- Component definitions --------------

    struct Transform3D {
        static constexpr Types kType = Types::TRFM3D;
        static constexpr const char* kName = "Transform3D"; // Government name

        glm::mat4 base = glm::mat4(1.0f);
        glm::mat4 local = glm::mat4(1.0f);
        glm::mat4 global = glm::mat4(1.0f);

        void init(const glm::mat4& mat) { base = mat; local = mat; }
        void set(const glm::mat4& mat) { local = mat; }
        void reset() { local = base; }
    };
    using TRFM3D = Transform3D;

    struct MeshRender3D {
        static constexpr Types kType = Types::MESHRD;
        static constexpr const char* kName = "MeshRender3D";
        tinyHandle pHandle;
    };
    using MESHRD = MeshRender3D;

    struct BoneAttach3D {
        static constexpr Types kType = Types::BONE3D;
        static constexpr const char* kName = "BoneAttach3D";
        tinyHandle skeleNodeHandle;
        uint32_t boneIndex;
    };
    using BONE3D = BoneAttach3D;

    struct Skeleton3D {
        static constexpr Types kType = Types::SKEL3D;
        static constexpr const char* kName = "Skeleton3D";
        tinyHandle pHandle;
    };
    using SKEL3D = Skeleton3D;

    struct Animation3D {
        static constexpr Types kType = Types::ANIM3D;
        static constexpr const char* kName = "Animation3D";
        tinyHandle pHandle;
    };
    using ANIM3D = Animation3D;

    struct Script {
        static constexpr Types kType = Types::SCRIPT;
        static constexpr const char* kName = "Script";
        tinyHandle pHandle;
    };
    using SCRIPT = Script; // Lol

// ----------- Component management functions -----------

    template<typename T>
    bool has() const {
        return hasType(T::kType);
    }

    template<typename T>
    T* add(const T& componentData) {
        setType(T::kType, true);
        getComponent<T>() = componentData;
        return &getComponent<T>();
    }

    template<typename T>
    T* add() {
        setType(T::kType, true);
        getComponent<T>() = T();
        return &getComponent<T>();
    }

    template<typename T>
    bool remove() {
        setType(T::kType, false);
        getComponent<T>() = T();
        return true;
    }

    template<typename T>
    T* get() { return has<T>() ? &getComponent<T>() : nullptr; }

    template<typename T>
    const T* get() const { return has<T>() ? &getComponent<T>() : nullptr; }

private:
    std::tuple<TRFM3D, MESHRD, BONE3D, SKEL3D, ANIM3D, SCRIPT> components;

    uint32_t types = 0;

    template<typename T>
    T& getComponent() { return std::get<T>(components); }

    template<typename T>
    const T& getComponent() const { return std::get<T>(components); }

    static constexpr uint32_t toMask(Types t) {
        return static_cast<uint32_t>(t);
    }

    void setType(Types componentType, bool state) {
        if (state) types |= toMask(componentType);
        else       types &= ~toMask(componentType);
    }

    bool hasType(Types componentType) const {
        return (types & toMask(componentType)) != 0;
    }
};

} // namespace tinyRT

using tinyNodeRT = tinyRT::Node;

// Aliases for component that can directly exist in Node instead of through pHandle
using tinyRT_TRFM3D = tinyNodeRT::Transform3D;
using tinyRT_BONE3D = tinyNodeRT::BoneAttach3D;