#pragma once

#include "TinyExt/TinyHandle.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <cstdint>
#include <tuple>

struct TinyNode {
    std::string name = "Node";

    enum class Scope {
        Local, // Local with model
        Global // Global registry
    } scope = Scope::Local;

    enum class Types : uint32_t {
        Node          = 1 << 0,
        MeshRender    = 1 << 1,
        Skeleton      = 1 << 2,
        BoneAttach    = 1 << 3
    };
    uint32_t types = toMask(Types::Node);

    TinyHandle parent;
    std::vector<TinyHandle> children;

    glm::mat4 transform = glm::mat4(1.0f);

    // Component definitions
    struct MeshRender {
        static constexpr Types kType = Types::MeshRender;
        TinyHandle mesh;
        std::vector<TinyHandle> submeshMats;
        TinyHandle skeleNode;
    };

    struct BoneAttach {
        static constexpr Types kType = Types::BoneAttach;
        TinyHandle skeleNode; // Point to a Skeleton node
        TinyHandle bone; // Index in skeleton
    };

    /** Keep in mind very clear distinction between:
    ** @param skeleNode: local index to the model's node array that contains the skeleton
        -> reference a node
    ** @param skeleRegistry: global index to the registry's skeleton array
        -> reference a skeleton
    */
    struct Skeleton {
        static constexpr Types kType = Types::Skeleton;
        TinyHandle skeleRegistry;
    };

protected: // RNode inherits from this

    // Store all components in a tuple - completely generic!
    std::tuple<MeshRender, BoneAttach, Skeleton> components;

    // Template magic to get component by type from tuple
    template<typename T>
    T& getComponent() {
        return std::get<T>(components);
    }

    template<typename T>
    const T& getComponent() const {
        return std::get<T>(components);
    }

public:
    static constexpr uint32_t toMask(Types t) {
        return static_cast<uint32_t>(t);
    }

    // Component management functions
    bool hasType(Types componentType) const {
        return (types & toMask(componentType)) != 0;
    }

    void setType(Types componentType, bool state) {
        if (state) types |= toMask(componentType);
        else       types &= ~toMask(componentType);
    }

    // Completely generic template functions - no knowledge of specific components!
    template<typename T>
    bool hasComponent() const {
        return hasType(T::kType);
    }

    template<typename T>
    void add(const T& componentData) {
        setType(T::kType, true);
        getComponent<T>() = componentData;
    }

    template<typename T>
    void remove() {
        setType(T::kType, false);
    }

    template<typename T>
    T* get() {
        return hasComponent<T>() ? &getComponent<T>() : nullptr;
    }

    template<typename T>
    const T* get() const {
        return hasComponent<T>() ? &getComponent<T>() : nullptr;
    }
};