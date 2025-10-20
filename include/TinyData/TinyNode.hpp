#pragma once

#include "TinyExt/TinyHandle.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <cstdint>
#include <tuple>

struct TinyNode {
    std::string name = "Node";
    TinyNode() = default;

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

    // Hierarchy data - can be either local indices or runtime handles depending on scope
    TinyHandle parentHandle;
    std::vector<TinyHandle> childrenHandles;

    // Transform data - both local and runtime
    glm::mat4 localTransform = glm::mat4(1.0f);   // Local/original transform
    glm::mat4 globalTransform = glm::mat4(1.0f);  // Runtime computed global transform

    // Component definitions with runtime capabilities
    struct MeshRender {
        static constexpr Types kType = Types::MeshRender;
        TinyHandle meshHandle;                // Handle to mesh in registry
        TinyHandle skeleNodeHandle;           // Handle to skeleton node (NOT skeleton in registry)
    };

    struct BoneAttach {
        static constexpr Types kType = Types::BoneAttach;
        TinyHandle skeleNodeHandle;
        uint32_t boneIndex;
    };

    struct Skeleton {
        static constexpr Types kType = Types::Skeleton;
        TinyHandle skeleHandle;   // Original skeleton data

        // Runtime skeleton data here
        std::vector<glm::mat4> localPose; // Manipulatable (by animation or by user)
        std::vector<glm::mat4> finalPose; // Final computed pose for skinning

        void resize(size_t boneCount) {
            localPose.resize(boneCount, glm::mat4(1.0f));
            finalPose.resize(boneCount, glm::mat4(1.0f));
        }
    };

private:
    std::tuple<MeshRender, BoneAttach, Skeleton> components;

    template<typename T>
    T& getComponent() { return std::get<T>(components); }

    template<typename T>
    const T& getComponent() const { return std::get<T>(components); }

    static constexpr uint32_t toMask(Types t) {
        return static_cast<uint32_t>(t);
    }
public:

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
    T* get() { return hasComponent<T>() ? &getComponent<T>() : nullptr; }

    template<typename T>
    const T* get() const { return hasComponent<T>() ? &getComponent<T>() : nullptr; }
};
