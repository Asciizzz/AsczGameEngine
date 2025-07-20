#pragma once

#include <AzVulk/Device.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // as opposed to -1 to 1
#include <glm/glm.hpp>

#include <array>

namespace AzVulk {

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions();
};

class Model {
public:
    Model(Device& device, const std::vector<Vertex>& vertices);
    ~Model(); void cleanup();

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    // void bind(VkCommandBuffer commandBuffer);
    // void draw(VkCommandBuffer commandBuffer);

private:
    void createVertexBuffer(const std::vector<Vertex>& vertices);

    Device& device;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    uint32_t vertexCount;
};

} // namespace AzVulk