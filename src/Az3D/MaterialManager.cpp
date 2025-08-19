#include "Az3D/MaterialManager.hpp"

#include "AzVulk/Device.hpp"

using namespace AzVulk;

namespace Az3D {

    MaterialManager::MaterialManager(const Device& vkDevice)
    : vkDevice(vkDevice) {
        auto defaultMaterial = MakeShared<Material>();
        materials.push_back(defaultMaterial);
    }

    size_t MaterialManager::addMaterial(const Material& material) {
        materials.push_back(MakeShared<Material>(material));

        return materials.size() - 1;
    }

    void MaterialManager::createGPUBufferDatas() {
        VkDevice device = vkDevice.device;
        VkPhysicalDevice physicalDevice = vkDevice.physicalDevice;

        gpuBufferDatas.resize(materials.size());

        for (size_t i = 0; i < materials.size(); ++i) {
            // Create staging buffer
            BufferData stagingBuffer;
            stagingBuffer.initVulkanDevice(device, physicalDevice);
            stagingBuffer.createBuffer(
                1, sizeof(MaterialUBO), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
            stagingBuffer.mappedData();

            MaterialUBO materialUBO(materials[i]->prop1);
            memcpy(stagingBuffer.mapped, &materialUBO, sizeof(MaterialUBO));
            
            // printf("Copying material %zu\n", i);

            gpuBufferDatas[i].initVulkanDevice(device, physicalDevice);
            gpuBufferDatas[i].createBuffer(
                1, sizeof(MaterialUBO),
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            );

            TemporaryCommand copyCmd(vkDevice, "TransferPool");

            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = 0;
            copyRegion.size = sizeof(MaterialUBO);
            
            vkCmdCopyBuffer(copyCmd.cmdBuffer, stagingBuffer.buffer, gpuBufferDatas[i].buffer, 1, &copyRegion);
        }
    }


    // Descriptor set creation
    void MaterialManager::createDescriptorSets(uint32_t maxFramesInFlight) {
        VkDevice device = vkDevice.device;

        dynamicDescriptor.init(device);
        VkDescriptorSetLayoutBinding binding = DynamicDescriptor::fastBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        dynamicDescriptor.createSetLayout({binding});

        uint32_t materialCount = static_cast<uint32_t>(materials.size());
        dynamicDescriptor.createPool({
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, materialCount}
        }, maxFramesInFlight * materialCount);

        // Descriptor sets creation
        std::vector<VkDescriptorSetLayout> layouts(maxFramesInFlight, dynamicDescriptor.setLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = dynamicDescriptor.pool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocInfo.pSetLayouts = layouts.data();

        for (size_t i = 0; i < materials.size(); ++i) {
            const auto& materialBuffer = gpuBufferDatas[i].buffer;

            std::vector<VkDescriptorSet> descriptorSets(maxFramesInFlight);

            if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) continue;

            // Prepare the write structures outside the loop
            VkDescriptorBufferInfo materialBufferInfo{};
            materialBufferInfo.buffer = materialBuffer;
            materialBufferInfo.offset = 0;
            materialBufferInfo.range = sizeof(MaterialUBO);

            for (uint32_t j = 0; j < maxFramesInFlight; ++j) {
                VkWriteDescriptorSet descriptorWrite{};
                descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrite.dstBinding = 0;
                descriptorWrite.dstArrayElement = 0;
                descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrite.descriptorCount = 1;
                descriptorWrite.pBufferInfo = &materialBufferInfo;
                descriptorWrite.dstSet = descriptorSets[j];

                vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

                dynamicDescriptor.sets.push_back(descriptorSets[j]);
            }
        }
    }

} // namespace Az3D
