#include "AzVulk/DescriptorManager.hpp"
#include "AzVulk/Buffer.hpp"
#include "Az3D/Az3D.hpp"
#include <stdexcept>

namespace AzVulk {

    DescriptorManager::DescriptorManager(VkDevice device) : device(device) {}

    DescriptorManager::~DescriptorManager() {}

    // Create two descriptor set layouts: set 0 (global UBO), set 1 (material UBO + texture)
    void DescriptorManager::createDescriptorSetLayouts(uint32_t maxFramesInFlight) {
        // For material
        materialDynamicDescriptor.init(device);
        materialDynamicDescriptor.maxFramesInFlight = maxFramesInFlight;

        VkDescriptorSetLayoutBinding materialUBOBinding = DynamicDescriptor::fastBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        VkDescriptorSetLayoutBinding textureBinding = DynamicDescriptor::fastBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

        materialDynamicDescriptor.createSetLayout({materialUBOBinding, textureBinding});

        // For global ubo
        globalDynamicDescriptor.init(device);
        globalDynamicDescriptor.maxFramesInFlight = maxFramesInFlight;

        VkDescriptorSetLayoutBinding globalUBOBinding = DynamicDescriptor::fastBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        globalDynamicDescriptor.createSetLayout({globalUBOBinding});
    }

    // Create two pools: one for global UBOs, one for material sets
    void DescriptorManager::createDescriptorPools(uint32_t maxMaterials) {
        // For material
        materialDynamicDescriptor.maxResources = maxMaterials;
        materialDynamicDescriptor.createPool({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER});

        // For GlobalUBO
        globalDynamicDescriptor.maxResources = 1;
        globalDynamicDescriptor.createPool({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER});
    }

    // Create material descriptor sets (set 1, per material per frame)
    void DescriptorManager::createMaterialDescriptorSets(const Az3D::Texture* texture, VkBuffer materialUniformBuffer, size_t materialIndex) {
        auto& matDesc = materialDynamicDescriptor;
        uint32_t maxFramesInFlight = matDesc.maxFramesInFlight;

        std::vector<VkDescriptorSetLayout> layouts(maxFramesInFlight, matDesc.setLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = matDesc.pool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocInfo.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> descriptorSets(maxFramesInFlight);
        if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate material descriptor sets for material index: " + std::to_string(materialIndex));
        }

        // Prepare the write structures outside the loop
        VkDescriptorBufferInfo materialBufferInfo{};
        materialBufferInfo.buffer = materialUniformBuffer;
        materialBufferInfo.offset = 0;
        materialBufferInfo.range = sizeof(MaterialUBO);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = texture->view;
        imageInfo.sampler = texture->sampler;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        // Fill in all fields except dstSet
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &materialBufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        for (uint32_t i = 0; i < maxFramesInFlight; ++i) {
            for (auto& write : descriptorWrites) write.dstSet = descriptorSets[i];

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }

        matDesc.mapSets[materialIndex] = std::move(descriptorSets);
    }

    void DescriptorManager::createGlobalDescriptorSets(const std::vector<VkBuffer>& uniformBuffers, size_t uniformBufferSize) {
        auto& glbDesc = globalDynamicDescriptor;

        std::vector<VkDescriptorSetLayout> layouts(glbDesc.maxFramesInFlight, glbDesc.setLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = glbDesc.pool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocInfo.pSetLayouts = layouts.data();

        glbDesc.sets.resize(glbDesc.maxFramesInFlight);
        if (vkAllocateDescriptorSets(device, &allocInfo, glbDesc.sets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate global descriptor sets");
        }

        for (uint32_t i = 0; i < glbDesc.maxFramesInFlight; ++i) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = uniformBufferSize;

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = glbDesc.sets[i];
            write.dstBinding = 0;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        }
    }




// DYNAMIC DESCRIPTOR SETS

    DynamicDescriptor::~DynamicDescriptor() {
        for (auto& set : sets) {
            vkFreeDescriptorSets(device, pool, 1, &set);
        }

        if (setLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
        if (pool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, pool, nullptr);
    }

    void DynamicDescriptor::createSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
        if (setLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
            setLayout = VK_NULL_HANDLE;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &setLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout");
        }
    }

    void DynamicDescriptor::createPool(const std::vector<VkDescriptorType>& types) {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, pool, nullptr);
            pool = VK_NULL_HANDLE;
        }

        for (const auto& type : types) {
            poolSizes.push_back({ type, maxResources * maxFramesInFlight });
        }

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = maxFramesInFlight * maxResources;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool");
        }
    }


// Helpful functions

    inline VkDescriptorSetLayoutBinding DynamicDescriptor::fastBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t descriptorCount) {
        VkDescriptorSetLayoutBinding bindingInfo{};
        bindingInfo.binding = binding;
        bindingInfo.descriptorCount = descriptorCount;
        bindingInfo.descriptorType = type;
        bindingInfo.pImmutableSamplers = nullptr;
        bindingInfo.stageFlags = stageFlags;
        return bindingInfo;
    }
}
