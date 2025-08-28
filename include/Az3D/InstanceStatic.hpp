#pragma once

#include <string>
#include <glm/glm.hpp>

#include "AzVulk/Buffer.hpp"
#include "Helpers/AzPair.hpp"

namespace Az3D {

// Dynamic, per-frame object data
struct InstanceStatic {
    InstanceStatic() = default;

    glm::uvec4 properties = glm::uvec4(0); // <materialIndex>, <indicator>, <empty>, <empty>
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    glm::vec4 multColor = glm::vec4(1.0f);

    // Vulkan-specific methods for vertex input
    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 6> getAttributeDescriptions();
};


struct InstanceStaticGroup {
    InstanceStaticGroup() = default;

    InstanceStaticGroup(const InstanceStaticGroup&) = delete;
    InstanceStaticGroup& operator=(const InstanceStaticGroup&) = delete;
    InstanceStaticGroup(InstanceStaticGroup&&) noexcept = default;
    InstanceStaticGroup& operator=(InstanceStaticGroup&&) noexcept = default;

    size_t prevInstanceCount = 0;
    std::vector<InstanceStatic> datas;
    size_t addInstance(const InstanceStatic& data);

    AzVulk::BufferData bufferData;
    void initVkDevice(const AzVulk::Device* vkDevice);
    void recreateBufferData();
    void updateBufferData();

    uint32_t meshIndex = SIZE_MAX; // We only need this value
};

} // namespace Az3D
