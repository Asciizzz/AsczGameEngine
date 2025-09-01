
#include "Az3D/ResourceGroup.hpp"

using namespace AzVulk;
using namespace Az3D;

void ResourceGroup::createMaterialBuffer() {
    VkDeviceSize bufferSize = sizeof(Material) * materials.size();

    // --- staging buffer (CPU visible) ---
    BufferData stagingBuffer;
    stagingBuffer.initVkDevice(vkDevice);
    stagingBuffer.setProperties(
        bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    stagingBuffer.createBuffer();

    stagingBuffer.uploadData(materials.data());

    // --- lDevice-local buffer (GPU only, STORAGE + DST) ---
    matBuffer.cleanup();

    matBuffer.initVkDevice(vkDevice);
    matBuffer.setProperties(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    matBuffer.createBuffer();

    // --- copy staging -> lDevice local ---
    TemporaryCommand copyCmd(vkDevice, vkDevice->transferPoolWrapper);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = bufferSize;

    vkCmdCopyBuffer(copyCmd.cmdBuffer, stagingBuffer.buffer, matBuffer.buffer, 1, &copyRegion);

    copyCmd.endAndSubmit();
}

// Descriptor set creation
void ResourceGroup::createMaterialDescSet() {
    VkDevice lDevice = vkDevice->lDevice;

    // Clear existing resources
    matDescPool.cleanup();
    matDescLayout.cleanup();
    matDescSet.cleanup();

    // Create descriptor pool and layout
    matDescPool.create(lDevice, { {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} }, 1);
    matDescLayout.create(lDevice, {
        DescLayout::BindInfo{
            0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT
        }
    });

    // Allocate descriptor set
    matDescSet.allocate(lDevice, matDescPool.get(), matDescLayout.get(), 1);

    // --- bind buffer to descriptor ---
    VkDescriptorBufferInfo materialBufferInfo{};
    materialBufferInfo.buffer = matBuffer.buffer;
    materialBufferInfo.offset = 0;
    materialBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = matDescSet.get();
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &materialBufferInfo;

    vkUpdateDescriptorSets(lDevice, 1, &descriptorWrite, 0, nullptr);
}