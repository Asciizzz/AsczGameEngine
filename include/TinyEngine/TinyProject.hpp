#pragma once

#include "TinyEngine/TinyRegistry.hpp"
#include "TinyEngine/TinyInstance.hpp"

struct TinyNodeRT {
    TinyHandle regHandle;     // Points to registry node (data for reference)
    TinyNode3D::Type type = TinyNode3D::Type::Node; // Direct copy of registry type

    uint32_t parentIdx = UINT32_MAX;    // Runtime parent index (UINT32_MAX = no parent)
    std::vector<uint32_t> childrenIdxs;   // Runtime children indices

    bool isDirty = true;

    // Transform data is now at the base level, like TinyNode3D
    glm::mat4 transformOverride = glm::mat4(1.0f);
    glm::mat4 globalTransform = glm::mat4(1.0f);

    // Runtime data structures
    struct Node {
        static constexpr TinyNode3D::Type kType = TinyNode3D::Type::Node;
        // No additional data needed for base node
    };

    struct Mesh {
        static constexpr TinyNode3D::Type kType = TinyNode3D::Type::MeshRender;

        // Overrideable indices
        uint32_t skeleNodeOverride = UINT32_MAX; // Point to a runtime Skeleton node (UINT32_MAX = no override)
    };

    struct Bone {
        static constexpr TinyNode3D::Type kType = TinyNode3D::Type::BoneAttach;

        uint32_t skeleNodeOverride = UINT32_MAX; // Point to a runtime Skeleton node (UINT32_MAX = no override)
    };

    struct Skeleton {
        static constexpr TinyNode3D::Type kType = TinyNode3D::Type::Skeleton;

        std::vector<glm::mat4> boneTransformsFinal; // Final bone transforms for skinning
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
    static TinyNodeRT make(const T& data) {
        TinyNodeRT node;
        using CleanT = std::decay_t<T>;   // remove refs/const
        node.type = CleanT::kType;
        node.data = data;
        return node;
    }

    /**
     * Manages parent-child relationships with automatic dirty flagging.
     * Adds child to this node and marks both nodes as dirty for transform updates.
     */
    void addChild(uint32_t childIndex, std::vector<std::unique_ptr<TinyNodeRT>>& allRuntimeNodes);
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

        auto rootNode = MakeUnique<TinyNodeRT>();
        rootNode->regHandle = rootHandle;
        // Children will be added upon scene population

        runtimeNodes.push_back(std::move(rootNode));
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
     * @param parentIndex point to the runtime node to inherit from (optional).
     */
    void addNodeInstance(uint32_t templateIndex, uint32_t parentIndex = 0);

    void printRuntimeNodeRecursive(
        const UniquePtrVec<TinyNodeRT>& runtimeNodes,
        TinyRegistry* registry,
        const TinyHandle& runtimeHandle,
        int depth = 0
    );

    void printRuntimeNodeHierarchy() {
        printRuntimeNodeRecursive(runtimeNodes, registry.get(), TinyHandle::make(0, TinyHandle::Type::Node, false));
    };

    void printRuntimeNodeOrdered();

    /**
     * Recursively updates global transforms for dirty nodes starting from the specified root.
     * Only processes nodes marked as isDirty = true, and sets isDirty = false after processing.
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
    const UniquePtrVec<TinyNodeRT>& getRuntimeNodes() const { return runtimeNodes; }

    const UniquePtr<TinyRegistry>& getRegistry() const { return registry; }

private:
    const AzVulk::DeviceVK* device;

    UniquePtr<TinyRegistry> registry;

    std::vector<TinyTemplate> templates;

    // A basic scene (best if we use smart pointers)
    UniquePtrVec<TinyNodeRT> runtimeNodes;
};