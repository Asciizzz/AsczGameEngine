#pragma once

#include "TinyEngine/TinyRegistry.hpp"

struct TinyNodeRuntime {
    TinyHandle regHandle;     // Points to registry node
    TinyHandle runtimeHandle; // Points to another runtime node

    // Override data (not yet implemented)
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

    uint32_t addTemplateFromModel(const TinyModelNew& model); // Returns template index + remapping a bunch of shit (very complex) (cops called)

private:
    const AzVulk::DeviceVK* device;

    UniquePtr<TinyRegistry> registry;

    std::vector<TinyHandle> templates; // Point to registry

    std::vector<TinyNodeRuntime> nodes; // Construct from registry[templates.type][template.index]
};