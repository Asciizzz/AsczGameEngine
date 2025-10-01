#pragma once

#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "TinyVK/DataBuffer.hpp"

namespace TinyEngine {

// Dynamic, per-frame object data
struct StaticInstance {
    StaticInstance() = default;

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

    uint32_t prevInstanceCount = 0;
    std::vector<StaticInstance> datas;
    size_t addInstance(const StaticInstance& data);

    VkDevice lDevice = VK_NULL_HANDLE;
    VkPhysicalDevice pDevice = VK_NULL_HANDLE;

    TinyVK::DataBuffer dataBuffer;
    void initVkDevice(const TinyVK::DeviceVK* deviceVK);
    void initVkDevice(VkDevice lDevice, VkPhysicalDevice pDevice);

    void recreateDataBuffer();
    void updateDataBuffer();

    uint32_t modelIndex = 0;
};

} // namespace TinyEngine
