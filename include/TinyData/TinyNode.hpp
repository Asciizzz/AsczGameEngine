#pragma once

#include "TinyEngine/TinyHandle.hpp"
#include "Helpers/Templates.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct TinyNode3D {
    // Default node data
    std::string name = "Node";

    enum class Scope {
        Local, // Local with model
        Global // Global registry
    } scope = Scope::Local;

    enum class Type {
        Node,
        MeshRender,
        Skeleton,
        BoneAttach
    } type = Type::Node;

    TinyHandle parent;
    std::vector<TinyHandle> children;

    // Transform data is now part of the base TinyNode3D
    glm::mat4 transform = glm::mat4(1.0f);

    struct Node {
        static constexpr Type kType = Type::Node;
    };

    struct MeshRender {
        static constexpr Type kType = Type::MeshRender;

        TinyHandle mesh;
        std::vector<TinyHandle> submeshMats;
        TinyHandle skeleNode;
    };

    struct BoneAttach {
        static constexpr Type kType = Type::BoneAttach;

        TinyHandle skeleNode; // Point to a Skeleton node
        TinyHandle bone; // Index in skeleton
    };

    struct Skeleton {
        static constexpr Type kType = Type::Skeleton;

        TinyHandle skeleRegistry;
    };  

    /* Keep in mind very clear distinction between:
        * skeleNode: local index to the model's node array that contains the skeleton
            -> reference a node
        * skeleRegistry: global index to the registry's skeleton array
            -> reference a skeleton
    */

    MonoVariant<
        Node,
        MeshRender,
        Skeleton,
        BoneAttach
    > data = Node();

    template<typename T>
    void make(T&& newData) {
        type = T::kType;
        data = std::forward<T>(newData);
    }

    template<typename T>
    static TinyNode3D make(const T& data) {
        TinyNode3D node;
        node.type = T::kType;
        node.data = data;
        return node;
    }

    template<typename T>
    T& as() { return std::get<T>(data); }
    template<typename T>
    const T& as() const { return std::get<T>(data); }

    bool isType(Type t) const { return type == t; }
};