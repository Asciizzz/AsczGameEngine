#pragma once

#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "AzVulk/Buffer.hpp"
#include "Helpers/AzPair.hpp"

namespace Az3D {

// Dynamic, per-frame object data
struct StaticInstance {
    StaticInstance() = default;

    glm::uvec4 properties = glm::uvec4(0); // materialIndex, indicator, empty, empty
    glm::vec4 trformT_S = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); // Translation (x,y,z) and Scale (w)
    glm::quat trformR = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Rotation (w,x,y,z), basically a vec4

    glm::vec4 multColor = glm::vec4(1.0f);

    void setTransform(const glm::vec3& position, const glm::quat& rotation, float scale=1.0f) {
        trformT_S = glm::vec4(position, scale);
        trformR = rotation;
    }

    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};


struct StaticInstanceGroup {
    StaticInstanceGroup() = default;

    StaticInstanceGroup(const StaticInstanceGroup&) = delete;
    StaticInstanceGroup& operator=(const StaticInstanceGroup&) = delete;
    StaticInstanceGroup(StaticInstanceGroup&&) noexcept = default;
    StaticInstanceGroup& operator=(StaticInstanceGroup&&) noexcept = default;

    size_t prevInstanceCount = 0;
    std::vector<StaticInstance> datas;
    size_t addInstance(const StaticInstance& data);

    AzVulk::BufferData bufferData;
    void initVkDevice(const AzVulk::Device* vkDevice);
    void recreateBufferData();
    void updateBufferData();

    uint32_t meshIndex = SIZE_MAX; // We only need this value
};

} // namespace Az3D
