#pragma once

#include "TinyEngine/TinyRegistry.hpp"
#include "TinyEngine/TinyInstance.hpp"

struct TinyNodeRT3D {
    TinyHandle regHandle;     // Points to registry node (data for reference)
    uint8_t types = TinyNode::toMask(TinyNode::Types::Node);

    uint32_t parentIdx = UINT32_MAX;    // Runtime parent index (UINT32_MAX = no parent)
    std::vector<uint32_t> childrenIdxs;   // Runtime children indices

    bool isDirty = true;

    glm::mat4 transformOverride = glm::mat4(1.0f);
    glm::mat4 globalTransform = glm::mat4(1.0f);

    struct MeshRender {
        static constexpr TinyNode::Types kType = TinyNode::Types::MeshRender;

        uint32_t skeleNodeRT = UINT32_MAX;
    };

    struct BoneAttach {
        static constexpr TinyNode::Types kType = TinyNode::Types::BoneAttach;

        uint32_t skeleNodeRT = UINT32_MAX;
    };

    struct Skeleton {
        static constexpr TinyNode::Types kType = TinyNode::Types::Skeleton;

        std::vector<glm::mat4> boneTransformsFinal;
    };

private:
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
    // Component management functions
    bool hasType(TinyNode::Types componentType) const {
        return (types & TinyNode::toMask(componentType)) != 0;
    }

    void setType(TinyNode::Types componentType, bool state) {
        if (state) types |= TinyNode::toMask(componentType);
        else       types &= ~TinyNode::toMask(componentType);
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
        TinyHandle rootHandle = registry->addNode(TinyNode());

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