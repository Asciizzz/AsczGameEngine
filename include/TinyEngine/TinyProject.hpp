#pragma once

#include "TinyEngine/TinyRegistry.hpp"

struct TinyNodeRuntime {
    TinyHandle regHandle;     // Points to registry node (data for reference)

    int parent = -1;           // Runtime parent index
    std::vector<int> children; // Runtime children indices

    // Info override
    struct Node3D_runtime {
        static constexpr TinyNode::Type kType = TinyNode::Type::Node3D;

        glm::mat4 transform = glm::mat4(1.0f);
    };

    struct Mesh3D_runtime : Node3D_runtime {
        static constexpr TinyNode::Type kType = TinyNode::Type::Mesh3D;

        // Overrideable handles
        std::vector<TinyHandle> submeshMatOverride;
        TinyHandle skeletonNodeOverride;
    };

    struct Skeleton3D_runtime : Node3D_runtime {
        static constexpr TinyNode::Type kType = TinyNode::Type::Skeleton3D;

        TinyHandle skeletonHandle; // Points to registry skeleton
    };

    MonoVariant<
        Node3D_runtime,
        Mesh3D_runtime,
        Skeleton3D_runtime
    > data = Node3D_runtime();
};

class TinyProject {
public:
    TinyProject(const AzVulk::DeviceVK* deviceVK) : device(deviceVK) {
        registry = MakeUnique<TinyRegistry>(deviceVK);
    }

    // Delete copy
    TinyProject(const TinyProject&) = delete;
    TinyProject& operator=(const TinyProject&) = delete;
    // No move semantics, where tf would you even want to move it to?

    // Return the template index, which in turn contains handles to the registry
    uint32_t addTemplateFromModel(const TinyModelNew& model); // Returns template index + remapping a bunch of shit (very complex) (cops called)

private:
    const AzVulk::DeviceVK* device;

    UniquePtr<TinyRegistry> registry;

    std::vector<TinyHandle> templates; // Point to registry (can be virtually anything)

    // A basic scene
    std::vector<TinyNodeRuntime> runtimeNodes; // Construct from registry[templates.type][template.index]
};