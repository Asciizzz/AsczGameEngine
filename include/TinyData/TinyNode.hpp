#pragma once

#include "TinyExt/TinyHandle.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <cstdint>
#include <tuple>

struct TinyNode {
    std::string name = "Node";
    TinyNode(const std::string& nodeName = "Node") : name(nodeName) {}

    enum class Types : uint32_t {
        Transform     = 1 << 0,
        MeshRender    = 1 << 1,
        Skeleton      = 1 << 2,
        BoneAttach    = 1 << 3,
        Animation     = 1 << 4
    };


    TinyHandle parentHandle;
    std::vector<TinyHandle> childrenHandles;

    void setParent(TinyHandle newParent) {
        parentHandle = newParent;
    }

    void addChild(TinyHandle childHandle) {
        childrenHandles.push_back(childHandle);
    }

    void removeChild(TinyHandle childHandle) {
        childrenHandles.erase(std::remove(childrenHandles.begin(), childrenHandles.end(), childHandle), childrenHandles.end());
    }

    // Transform data - both local and runtime
    struct Transform {
        static constexpr Types kType = Types::Transform;
        glm::mat4 local = glm::mat4(1.0f);
        glm::mat4 global = glm::mat4(1.0f);
    };

    // Component definitions with runtime capabilities
    struct MeshRender {
        static constexpr Types kType = Types::MeshRender;
        TinyHandle meshHandle;      // Handle to mesh in registry
        TinyHandle skeleNodeHandle; // Handle to skeleton "NODE" (NOT skeleton in registry)
    };

    struct BoneAttach {
        static constexpr Types kType = Types::BoneAttach;
        TinyHandle skeleNodeHandle;
        uint32_t boneIndex;
    };

    struct Skeleton {
        static constexpr Types kType = Types::Skeleton;
        TinyHandle pSkeleHandle; // Polytype
    };

    struct Animation {
        static constexpr Types kType = Types::Animation;
        TinyHandle pAnimeHandle; // Polytype
    };

    // Component management functions

    // Completely generic template functions - no knowledge of specific components!
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
        return true;
    }

    template<typename T>
    T* get() { return has<T>() ? &getComponent<T>() : nullptr; }

    template<typename T>
    const T* get() const { return has<T>() ? &getComponent<T>() : nullptr; }

private:
    std::tuple<
        Transform,
        MeshRender,
        BoneAttach,
        Skeleton,
        Animation
    > components;

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
