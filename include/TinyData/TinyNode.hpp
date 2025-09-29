#pragma once

#include <Helpers/Templates.hpp>
#include <cstdint>

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
        Mesh3D,
        Skeleton3D
    } type = Type::Node3D;

    int parent = -1;
    std::vector<int> children;

    struct Node3D {
        static constexpr Type kType = Type::Node3D;

        glm::mat4 transform = glm::mat4(1.0f);
    };

    struct Mesh3D : Node3D {
        static constexpr Type kType = Type::Mesh3D;

        int meshIndex = -1;
        std::vector<int> submeshMats;
        int skeletonNodeIndex = -1;
    };

    struct Skeleton3D : Node3D {
        static constexpr Type kType = Type::Skeleton3D;

        int skeletonRegIndex = -1;
    };

    /* Keep in mind very clear distinction between:
        * skeletonNodeIndex: local index to the model's node array that contains the skeleton
            -> reference a node
        * skeletonRegIndex: global index to the registry's skeleton array
            -> reference a skeleton
    */

    MonoVariant<
        Node3D,
        Mesh3D,
        Skeleton3D
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

    static TinyNode make(glm::mat4 transform = glm::mat4(1.0f)) {
        TinyNode node;
        node.type = Type::Node3D;
        node.data = Node3D{ transform };
        return node;
    }

    // Convenience helpers
    bool isNode3D() const { return type == Type::Node3D; }
    bool isMesh3D() const { return type == Type::Mesh3D; }
    bool isSkeleton3D() const { return type == Type::Skeleton3D; }

    const Node3D& asNode3D() const { return std::get<Node3D>(data); }
    const Mesh3D& asMesh3D() const { return std::get<Mesh3D>(data); }
    const Skeleton3D& asSkeleton3D() const { return std::get<Skeleton3D>(data); }
};