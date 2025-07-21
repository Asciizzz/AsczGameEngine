#include <AzVulk/Model.hpp>

namespace AzVulk {

VkVertexInputBindingDescription Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 2> Vertex::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT; // glm::vec2
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT; // glm::vec3
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    return attributeDescriptions;
}



Model::Model(
    Device& device,
    const std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices
) : device(device) {
    createVertexBuffer(vertices);
    createIndexBuffer(indices);
}
void Model::cleanup() {
    vkDestroyBuffer(device.getDevice(), vertexBuffer, nullptr);
    vkFreeMemory(device.getDevice(), vertexBufferMemory, nullptr);

    vkDestroyBuffer(device.getDevice(), indexBuffer, nullptr);
    vkFreeMemory(device.getDevice(), indexBufferMemory, nullptr);
}


void Model::createVertexBuffer(const std::vector<Vertex>& vertices) {
    vertexCount = static_cast<uint32_t>(vertices.size());
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    Helper::CreateBuffer(
        device.getDevice(), device.getPhysicalDevice(),
        bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory
    );

    void* data;
    vkMapMemory(device.getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device.getDevice(), stagingBufferMemory);

    Helper::CreateBuffer(
        device.getDevice(), device.getPhysicalDevice(),
        bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBuffer, vertexBufferMemory
    );

    Helper::CopyBuffer(
        device.getDevice(), device.getCommandPool(), device.getGraphicsQueue(),
        stagingBuffer, vertexBuffer, bufferSize
    );

    vkDestroyBuffer(device.getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(device.getDevice(), stagingBufferMemory, nullptr);
}

void Model::createIndexBuffer(const std::vector<uint32_t>& indices) {
    indexCount = static_cast<uint32_t>(indices.size());
    VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    Helper::CreateBuffer(
        device.getDevice(), device.getPhysicalDevice(),
        bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory
    );

    void* data;
    vkMapMemory(device.getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(device.getDevice(), stagingBufferMemory);

    Helper::CreateBuffer(
        device.getDevice(), device.getPhysicalDevice(),
        bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBuffer, indexBufferMemory
    );

    Helper::CopyBuffer(
        device.getDevice(), device.getCommandPool(), device.getGraphicsQueue(),
        stagingBuffer, indexBuffer, bufferSize
    );

    vkDestroyBuffer(device.getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(device.getDevice(), stagingBufferMemory, nullptr);
}

} // namespace AzVulk