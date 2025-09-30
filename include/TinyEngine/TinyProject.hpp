#pragma once

#include "TinyEngine/TinyRegistry.hpp"
#include "TinyEngine/TinyInstance.hpp"

struct TinyNodeRT3D {
    TinyHandle regHandle;     // Points to registry node (data for reference)
    TinyNode3D::Type type = TinyNode3D::Type::Node; // Direct copy of registry type

    uint32_t parentIdx = UINT32_MAX;    // Runtime parent index (UINT32_MAX = no parent)
    std::vector<uint32_t> childrenIdxs;   // Runtime children indices

    bool isDirty = true;

    glm::mat4 transformOverride = glm::mat4(1.0f);
    glm::mat4 globalTransform = glm::mat4(1.0f);

    // Runtime data structures
    struct Node {
        static constexpr TinyNode3D::Type kType = TinyNode3D::Type::Node;
    };

    struct Mesh {
        static constexpr TinyNode3D::Type kType = TinyNode3D::Type::MeshRender;

        uint32_t skeleNodeRT = UINT32_MAX;
    };

    struct Bone {
        static constexpr TinyNode3D::Type kType = TinyNode3D::Type::BoneAttach;

        uint32_t skeleNodeRT = UINT32_MAX;
    };

    struct Skeleton {
        static constexpr TinyNode3D::Type kType = TinyNode3D::Type::Skeleton;

        std::vector<glm::mat4> boneTransformsFinal;
    };

    MonoVariant<
        Node,
        Mesh,
        Skeleton
    > data = Node();

    template<typename T>
    void make(T&& newData) {
        using CleanT = std::decay_t<T>;   // remove refs/const
        type = CleanT::kType;
        data = std::forward<T>(newData);
    }

    template<typename T>
    static TinyNodeRT3D make(const T& data) {
        TinyNodeRT3D node;
        using CleanT = std::decay_t<T>;   // remove refs/const
        node.type = CleanT::kType;
        node.data = data;
        return node;
    }

    /**
     * Manages parent-child relationships with automatic dirty flagging.
     * Adds child to this node and marks both nodes as dirty for transform updates.
     */
    void addChild(uint32_t childIndex, std::vector<std::unique_ptr<TinyNodeRT3D>>& allrtNodes);
};

struct TinyTemplate {
    std::vector<TinyHandle> registryNodes;

    TinyTemplate() = default;
    TinyTemplate(const std::vector<TinyHandle>& nodes) : registryNodes(nodes) {}
};

class TinyProject {
public:
    TinyProject(const AzVulk::DeviceVK* deviceVK) : device(deviceVK) {
        registry = MakeUnique<TinyRegistry>(deviceVK);

        // Create root node
        TinyHandle rootHandle = registry->addNode(TinyNode3D());

        auto rootNode = MakeUnique<TinyNodeRT3D>();
        rootNode->regHandle = rootHandle;
        // Children will be added upon scene population

        rtNodes.push_back(std::move(rootNode));
    }

    // Delete copy
    TinyProject(const TinyProject&) = delete;
    TinyProject& operator=(const TinyProject&) = delete;
    // No move semantics, where tf would you even want to move it to?

    // Return the template index, which in turn contains handles to the registry
    uint32_t addTemplateFromModel(const TinyModelNew& model); // Returns template index + remapping a bunch of shit (very complex) (cops called)

    /**
     * Adds a node instance to the scene.
     *
     * @param templateIndex point to the template to use.
     * @param rootIndex point to the runtime node to inherit from (optional).
     */
    void addNodeInstance(uint32_t templateIndex, uint32_t rootIndex = 0);

    void printRuntimeNodeRecursive(
        const UniquePtrVec<TinyNodeRT3D>& rtNodes,
        TinyRegistry* registry,
        const TinyHandle& runtimeHandle,
        int depth = 0
    );

    void printRuntimeNodeHierarchy() {
        printRuntimeNodeRecursive(rtNodes, registry.get(), TinyHandle::make(0, TinyHandle::Type::Node, false));
    };

    void printRuntimeNodeOrdered();

    /**
     * Recursively updates global transforms for all nodes starting from the specified root.
     * Always processes all nodes regardless of dirty state for guaranteed correctness.
     * 
     * @param rootNodeIndex Index to the root node to start the recursive update from
     * @param parentGlobalTransform Global transform of the parent (identity for root nodes)
     */
    void updateGlobalTransforms(uint32_t rootNodeIndex, const glm::mat4& parentGlobalTransform = glm::mat4(1.0f));

    void printDataCounts() const {
        registry->printDataCounts();
    }

    /**
     * Playground function for testing - rotates root node by 90 degrees per second
     * @param dTime Delta time in seconds
     */
    void runPlayground(float dTime);

    // These are not official public methods, only for testing purposes
    const UniquePtrVec<TinyNodeRT3D>& getRuntimeNodes() const { return rtNodes; }

    const UniquePtr<TinyRegistry>& getRegistry() const { return registry; }

private:
    const AzVulk::DeviceVK* device;

    UniquePtr<TinyRegistry> registry;

    std::vector<TinyTemplate> templates;

    // A basic scene (best if we use smart pointers)
    UniquePtrVec<TinyNodeRT3D> rtNodes;
};