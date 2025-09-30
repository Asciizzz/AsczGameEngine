#pragma once

#include "TinyEngine/TinyRegistry.hpp"

struct TinyNodeRuntime {
    TinyHandle regHandle;     // Points to registry node (data for reference)
    TinyNode::Type type = TinyNode::Type::Node3D; // Direct copy of registry type

    TinyHandle parent;           // Runtime parent index
    std::vector<TinyHandle> children; // Runtime children indices

    bool isDirty = true;

    // Info override
    struct Node3D_runtime {
        static constexpr TinyNode::Type kType = TinyNode::Type::Node3D;

        glm::mat4 transformOverride = glm::mat4(1.0f);
        glm::mat4 globalTransform = glm::mat4(1.0f);
    };

    struct Mesh3D_runtime : Node3D_runtime {
        static constexpr TinyNode::Type kType = TinyNode::Type::MeshRender3D;

        // Overrideable handles (initialized with registry data)
        std::vector<glm::mat4> submeshTransformsOverride; // Per-submesh transform overrides
        std::vector<TinyHandle> submeshMatsOverride;
        TinyHandle skeletonNodeOverride;
    };

    struct Skeleton3D_runtime : Node3D_runtime {
        static constexpr TinyNode::Type kType = TinyNode::Type::Skeleton3D;

        TinyHandle skeletonHandle; // Points to registry skeleton
        std::vector<glm::mat4> boneTransformsFinal; // Final bone transforms for skinning
    };

    MonoVariant<
        Node3D_runtime,
        Mesh3D_runtime,
        Skeleton3D_runtime
    > data = Node3D_runtime();

    template<typename T>
    void make(T&& newData) {
        using CleanT = std::decay_t<T>;   // remove refs/const
        type = CleanT::kType;
        data = std::forward<T>(newData);
    }

    template<typename T>
    static TinyNodeRuntime make(const T& data) {
        TinyNodeRuntime node;
        using CleanT = std::decay_t<T>;   // remove refs/const
        node.type = CleanT::kType;
        node.data = data;
        return node;
    }
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
        TinyHandle rootHandle = registry->addNode(TinyNode());

        auto rootNode = MakeUnique<TinyNodeRuntime>();
        rootNode->regHandle = rootHandle;
        rootNode->parent = TinyHandle::invalid(); // No parent
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
     * @param inheritIndex point to the runtime node to inherit from (optional).
     */
    void addNodeInstance(uint32_t templateIndex, uint32_t inheritIndex = 0);

    void printRuntimeNodeRecursive(
        const UniquePtrVec<TinyNodeRuntime>& runtimeNodes,
        TinyRegistry* registry,
        const TinyHandle& runtimeHandle,
        int depth = 0
    );

    void printRuntimeNodeHierarchy() {
        printRuntimeNodeRecursive(runtimeNodes, registry.get(), TinyHandle::make(0, TinyHandle::Type::Node, false));
    };

    void printRuntimeNodeOrdered();

    void printDataCounts() const {
        registry->printDataCounts();
    }

private:
    const AzVulk::DeviceVK* device;

    UniquePtr<TinyRegistry> registry;

    std::vector<TinyTemplate> templates;

    // A basic scene (best if we use smart pointers)
    UniquePtrVec<TinyNodeRuntime> runtimeNodes; // Construct from registry[templates.type][template.index]
};