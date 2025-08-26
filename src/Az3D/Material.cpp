#include "Az3D/Material.hpp"

#include <stdexcept>

using namespace AzVulk;

namespace Az3D {

    MaterialManager::MaterialManager(const Device* vkDevice)
    : vkDevice(vkDevice) {
        materials.push_back(Material());
    }

    size_t MaterialManager::addMaterial(const Material& material) {
        materials.push_back(material);
        return materials.size() - 1;
    }

    void MaterialManager::createGPUBufferData() {
        VkDevice device = vkDevice->device;
        VkPhysicalDevice physicalDevice = vkDevice->physicalDevice;

        VkDeviceSize bufferSize = sizeof(Material) * materials.size();

        printf("Creating material buffer with %zu materials (%llu bytes)\n", materials.size(), bufferSize);

        // --- staging buffer (CPU visible) ---
        BufferData stagingBuffer;
        stagingBuffer.initVkDevice(vkDevice);
        stagingBuffer.setProperties(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        stagingBuffer.createBuffer();

        stagingBuffer.uploadData(materials.data());

        // --- device-local buffer (GPU only, STORAGE + DST) ---
        bufferData.initVkDevice(vkDevice);
        bufferData.setProperties(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        bufferData.createBuffer();

        // --- copy staging -> device local ---
        TemporaryCommand copyCmd(vkDevice, vkDevice->transferPoolWrapper);

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = bufferSize;

        vkCmdCopyBuffer(copyCmd.cmdBuffer, stagingBuffer.buffer, bufferData.buffer, 1, &copyRegion);
    }

    // Descriptor set creation
    void MaterialManager::createDescriptorSets() {
        VkDevice device = vkDevice->device;

        // --- create layout ---
        dynamicDescriptor.init(device);
        VkDescriptorSetLayoutBinding binding =
            DynamicDescriptor::fastBinding(
                0,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            );
        dynamicDescriptor.createLayout({binding});

        // --- create pool ---
        // Only 1 SSBO descriptor and 1 set are needed, because the SSBO holds ALL materials
        dynamicDescriptor.createPool(
            { VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} },
            1
        );

        // --- allocate descriptor set ---
        VkDescriptorSetLayout layout = dynamicDescriptor.setLayout;

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = dynamicDescriptor.pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor set for materials");
        }

        // --- bind buffer to descriptor ---
        VkDescriptorBufferInfo materialBufferInfo{};
        materialBufferInfo.buffer = bufferData.buffer;
        materialBufferInfo.offset = 0;
        materialBufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &materialBufferInfo;

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

        dynamicDescriptor.set = descriptorSet;
    }

    void MaterialManager::uploadToGPU() {
        createGPUBufferData();
        createDescriptorSets();
    }

} // namespace Az3D
