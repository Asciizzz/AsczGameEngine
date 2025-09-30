#pragma once

#include "TinyEngine/TinyHandle.hpp"
#include "Helpers/Templates.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct TinyNode {
    // Default node data
    std::string name = "Node";

    enum class Scope {
        Local, // Local with model
        Global // Global registry
    } scope = Scope::Local;

    enum class Type {
        Node3D,
        MeshRender3D,
        Skeleton3D,
        BoneAttach3D
    } type = Type::Node3D;

    TinyHandle parent;
    std::vector<TinyHandle> children;

    struct Node3D {
        static constexpr Type kType = Type::Node3D;

        glm::mat4 transform = glm::mat4(1.0f);
    };

    struct MeshRender3D : Node3D {
        static constexpr Type kType = Type::MeshRender3D;

        TinyHandle mesh;
        std::vector<TinyHandle> submeshMats;
        TinyHandle skeleNode;
    };

    struct BoneAttach3D : Node3D {
        static constexpr Type kType = Type::BoneAttach3D;

        TinyHandle skeleNode; // Point to a Akeleton3D node
        TinyHandle boneIndex = -1; // Index in skeleton
    };

    struct Skeleton3D : Node3D {
        static constexpr Type kType = Type::Skeleton3D;

        TinyHandle skeleRegistry;
    };  

    /* Keep in mind very clear distinction between:
        * skeleNode: local index to the model's node array that contains the skeleton
            -> reference a node
        * skeleRegistry: global index to the registry's skeleton array
            -> reference a skeleton
    */

    MonoVariant<
        Node3D,
        MeshRender3D,
        Skeleton3D,
        BoneAttach3D
    > data = Node3D();

    template<typename T>
    void make(T&& newData) {
        type = T::kType;
        data = std::forward<T>(newData);
    }

    template<typename T>
    static TinyNode make(const T& data) {
        TinyNode node;
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