#pragma once

#include "TinyData/TinyNode.hpp"
#include "TinyData/TinyMesh.hpp"

#include "AzVulk/DataBuffer.hpp"

class TinyInstance {
public:
    struct MeshTransform {
        glm::mat4 model = glm::mat4(1.0f);

        static VkVertexInputBindingDescription getBindingDescription();
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    };

    AzVulk::DataBuffer buffer;
    void initBuffer(const AzVulk::DeviceVK* deviceVK, size_t instanceCount);
};