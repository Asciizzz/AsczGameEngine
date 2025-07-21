#pragma once

#include <AzVulk/Buffer.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // as opposed to -1 to 1
#include <glm/glm.hpp>

#include <array>

namespace AzVulk {

class Model {
public:
    Model(
        Device& device,
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices
    );
    void cleanup();

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) = delete;
    Model& operator=(Model&&) = delete;

    // void bind(VkCommandBuffer commandBuffer);
    // void draw(VkCommandBuffer commandBuffer);

    VkBuffer getVertexBuffer() const { return vertexBuffer; }
    VkBuffer getIndexBuffer() const { return indexBuffer; }

private:
    Device& device;

    uint32_t vertexCount;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    void createVertexBuffer(const std::vector<Vertex>& vertices);

    uint32_t indexCount;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    void createIndexBuffer(const std::vector<uint32_t>& indices);
};

} // namespace AzVulk