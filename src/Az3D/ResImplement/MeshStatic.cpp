#include "Az3D/ResourceGroup.hpp"

using namespace AzVulk;
using namespace Az3D;


void ResourceGroup::createMeshStaticBuffers() {
    for (int i = 0; i < meshStatics.size(); ++i) {
        const auto* mesh = meshStatics[i].get();
        const auto& vertices = mesh->vertices;
        const auto& indices = mesh->indices;
        
        SharedPtr<BufferData> vBufferData = MakeShared<BufferData>();
        SharedPtr<BufferData> iBufferData = MakeShared<BufferData>();

        // Upload vertex
        BufferData vertexStagingBuffer;
        vertexStagingBuffer.initVkDevice(vkDevice);
        vertexStagingBuffer.setProperties(
            vertices.size() * sizeof(VertexStatic), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        vertexStagingBuffer.createBuffer();
        vertexStagingBuffer.mappedData(vertices.data());

        vBufferData->initVkDevice(vkDevice);
        vBufferData->setProperties(
            vertices.size() * sizeof(VertexStatic),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        vBufferData->createBuffer();

        TemporaryCommand vertexCopyCmd(vkDevice, vkDevice->transferPoolWrapper);

        VkBufferCopy vertexCopyRegion{};
        vertexCopyRegion.srcOffset = 0;
        vertexCopyRegion.dstOffset = 0;
        vertexCopyRegion.size = vertices.size() * sizeof(VertexStatic);

        vkCmdCopyBuffer(vertexCopyCmd.cmdBuffer, vertexStagingBuffer.buffer, vBufferData->buffer, 1, &vertexCopyRegion);
        vBufferData->hostVisible = false;

        vertexCopyCmd.endAndSubmit();

        // Upload index

        BufferData indexStagingBuffer;
        indexStagingBuffer.initVkDevice(vkDevice);
        indexStagingBuffer.setProperties(
            indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        indexStagingBuffer.createBuffer();
        indexStagingBuffer.mappedData(indices.data());

        iBufferData->initVkDevice(vkDevice);
        iBufferData->setProperties(
            indices.size() * sizeof(uint32_t),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        iBufferData->createBuffer();

        TemporaryCommand indexCopyCmd(vkDevice, vkDevice->transferPoolWrapper);

        VkBufferCopy indexCopyRegion{};
        indexCopyRegion.srcOffset = 0;
        indexCopyRegion.dstOffset = 0;
        indexCopyRegion.size = indices.size() * sizeof(uint32_t);

        vkCmdCopyBuffer(indexCopyCmd.cmdBuffer, indexStagingBuffer.buffer, iBufferData->buffer, 1, &indexCopyRegion);
        iBufferData->hostVisible = false;

        indexCopyCmd.endAndSubmit();


        // Append buffers
        vstaticBuffers.push_back(vBufferData);
        istaticBuffers.push_back(iBufferData);
    }
}