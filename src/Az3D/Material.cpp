#include "Az3D/Material.hpp"

#include "AzVulk/Device.hpp"

using namespace AzVulk;

namespace Az3D {

    MaterialManager::MaterialManager(const Device* vkDevice)
    : vkDevice(vkDevice) {
        auto defaultMaterial = MakeShared<Material>();
        materials.push_back(defaultMaterial);
    }

    size_t MaterialManager::addMaterial(const Material& material) {
        materials.push_back(MakeShared<Material>(material));

        return materials.size() - 1;
    }

    void MaterialManager::createGPUBufferDatas() {
        VkDevice device = vkDevice->device;
        VkPhysicalDevice physicalDevice = vkDevice->physicalDevice;

        gpuBufferDatas.resize(materials.size());

        for (size_t i = 0; i < materials.size(); ++i) {
            MaterialUBO materialUBO(materials[i]->prop1);

            // Create staging buffer
            BufferData stagingBuffer;
            stagingBuffer.initVkDevice(vkDevice);
            stagingBuffer.setProperties(
                sizeof(MaterialUBO), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
            stagingBuffer.createBuffer();
            stagingBuffer.mappedData(&materialUBO);

            gpuBufferDatas[i].initVkDevice(vkDevice);
            gpuBufferDatas[i].setProperties(
                sizeof(MaterialUBO),
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            );
            gpuBufferDatas[i].createBuffer();

            TemporaryCommand copyCmd(vkDevice, "Default_Transfer");

            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = 0;
            copyRegion.size = sizeof(MaterialUBO);
            
            vkCmdCopyBuffer(copyCmd.cmdBuffer, stagingBuffer.buffer, gpuBufferDatas[i].buffer, 1, &copyRegion);
            gpuBufferDatas[i].hostVisible = false;
        }
    }


    // Descriptor set creation
    void MaterialManager::createDescriptorSets() {
        VkDevice device = vkDevice->device;

        dynamicDescriptor.init(device);
        VkDescriptorSetLayoutBinding binding = DynamicDescriptor::fastBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        dynamicDescriptor.createLayout({binding});

        uint32_t materialCount = static_cast<uint32_t>(materials.size());
        dynamicDescriptor.createPool({
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, materialCount}
        }, materialCount);

        // Descriptor sets creation
        VkDescriptorSetLayout layout = dynamicDescriptor.setLayout;

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = dynamicDescriptor.pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        for (size_t i = 0; i < materials.size(); ++i) {
            const auto& materialBuffer = gpuBufferDatas[i].buffer;

            VkDescriptorSet descriptorSet;

            if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) continue;

            // Prepare the write structures outside the loop
            VkDescriptorBufferInfo materialBufferInfo{};
            materialBufferInfo.buffer = materialBuffer;
            materialBufferInfo.offset = 0;
            materialBufferInfo.range = sizeof(MaterialUBO);

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &materialBufferInfo;
            descriptorWrite.dstSet = descriptorSet;

            vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

            dynamicDescriptor.sets.push_back(descriptorSet);
        }
    }

    void MaterialManager::uploadToGPU() {
        createGPUBufferDatas();
        createDescriptorSets();
    }

} // namespace Az3D
