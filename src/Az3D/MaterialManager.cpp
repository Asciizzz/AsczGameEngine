#include "Az3D/MaterialManager.hpp"

namespace Az3D {

    size_t MaterialManager::addMaterial(const Material& material) {
        materials.push_back(MakeShared<Material>(material));

        return materials.size() - 1;
    }

    void MaterialManager::createBufferDatas(VkDevice device, VkPhysicalDevice physicalDevice) {
        using namespace AzVulk;

        bufferDatas.resize(materials.size());

        for (size_t i = 0; i < materials.size(); ++i) {
            auto& bufferData = bufferDatas[i];
            bufferData.initVulkan(device, physicalDevice);

            bufferData.createBuffer(
                1, sizeof(MaterialUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );

            MaterialUBO materialUBO(materials[i]->prop1);
            bufferData.uploadData(&materialUBO);
        }
    }


    // Descriptor set creation
    void MaterialManager::createDescriptorSets(VkDevice device, uint32_t maxFramesInFlight) {
        using namespace AzVulk;

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
            const auto& materialBuffer = bufferDatas[i].buffer;

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
