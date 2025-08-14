#include "AzVulk/DescriptorManager.hpp"
#include "AzVulk/Buffer.hpp"
#include "Az3D/Az3D.hpp"
#include <stdexcept>

namespace AzVulk {

    DescriptorManager::DescriptorManager(VkDevice device) : device(device) {}

    DescriptorManager::~DescriptorManager() {}


    void DescriptorManager::createDescriptorSetLayouts(uint32_t maxFramesInFlight) {
        // For global ubo + depth sampler
        globalDynamicDescriptor.init(device, maxFramesInFlight);
        VkDescriptorSetLayoutBinding globalUBOBinding = DynamicDescriptor::fastBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        VkDescriptorSetLayoutBinding depthSamplerBinding = DynamicDescriptor::fastBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        globalDynamicDescriptor.createSetLayout({globalUBOBinding, depthSamplerBinding});

        // For material
        materialDynamicDescriptor.init(device, maxFramesInFlight);
        VkDescriptorSetLayoutBinding materialUBOBinding = DynamicDescriptor::fastBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        materialDynamicDescriptor.createSetLayout({materialUBOBinding});

        // For texture
        textureDynamicDescriptor.init(device, maxFramesInFlight);
        VkDescriptorSetLayoutBinding textureBinding = DynamicDescriptor::fastBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        textureDynamicDescriptor.createSetLayout({textureBinding});
    }

    void DescriptorManager::createDescriptorPools(uint32_t maxMaterials, uint32_t maxTextures) {
        globalDynamicDescriptor.createPool(1, {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER});
        materialDynamicDescriptor.createPool(maxMaterials, {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER});
        textureDynamicDescriptor.createPool(maxTextures, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER});
    }


    void DynamicDescriptor::createGlobalDescriptorSets(
        const std::vector<VkBuffer>& uniformBuffers,
        size_t uniformBufferSize,
        VkImageView depthImageView,
        VkSampler depthSampler
    ) {
        std::vector<VkDescriptorSetLayout> layouts(maxFramesInFlight, setLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocInfo.pSetLayouts = layouts.data();

        for (auto& set : sets) {
            vkFreeDescriptorSets(device, pool, 1, &set);
            set = VK_NULL_HANDLE;
        }

        sets.resize(maxFramesInFlight);
        if (vkAllocateDescriptorSets(device, &allocInfo, sets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate global descriptor sets");
        }

        for (uint32_t i = 0; i < maxFramesInFlight; ++i) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = uniformBufferSize;

            VkDescriptorImageInfo depthInfo{};
            depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            depthInfo.imageView = depthImageView;
            depthInfo.sampler = depthSampler;

            std::array<VkWriteDescriptorSet, 2> writes{};

            // UBO
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = sets[i];
            writes[0].dstBinding = 0;
            writes[0].dstArrayElement = 0;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].descriptorCount = 1;
            writes[0].pBufferInfo = &bufferInfo;

            // Depth sampler
            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = sets[i];
            writes[1].dstBinding = 1;
            writes[1].dstArrayElement = 0;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].descriptorCount = 1;
            writes[1].pImageInfo = &depthInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }


    // Create material descriptor sets (set 1, per material per frame)

    // Legacy function (wrong)
    void DynamicDescriptor::createMaterialDescriptorSets_LEGACY(const Az3D::Texture* texture, VkBuffer materialUniformBuffer, size_t materialIndex) {
        std::vector<VkDescriptorSetLayout> layouts(maxFramesInFlight, setLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool;
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

        // mapSets[materialIndex] = std::move(descriptorSets);
    }

    void DynamicDescriptor::createMaterialDescriptorSets(
        const std::vector<std::shared_ptr<Az3D::Material>>& materials,
        const std::vector<BufferData>& materialUniformBuffers
    ) {
        std::vector<VkDescriptorSetLayout> layouts(maxFramesInFlight, setLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocInfo.pSetLayouts = layouts.data();

        for (size_t i = 0; i < materials.size(); ++i) {
            const auto& material = materials[i];
            const auto& materialUniformBuffer = materialUniformBuffers[i].buffer;

            std::vector<VkDescriptorSet> descriptorSets(maxFramesInFlight);

            if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) continue;

            // Prepare the write structures outside the loop
            VkDescriptorBufferInfo materialBufferInfo{};
            materialBufferInfo.buffer = materialUniformBuffer;
            materialBufferInfo.offset = 0;
            materialBufferInfo.range = sizeof(MaterialUBO);

            VkWriteDescriptorSet descriptorWrite;
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &materialBufferInfo;

            for (uint32_t j = 0; j < maxFramesInFlight; ++j) {
                descriptorWrite.dstSet = descriptorSets[j];

                vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
            }

            manySets.push_back(std::move(descriptorSets));
        }
    }

    void DynamicDescriptor::createTextureDescriptorSets(const std::vector<Az3D::Texture>& textures) {
        std::vector<VkDescriptorSetLayout> layouts(maxFramesInFlight, setLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocInfo.pSetLayouts = layouts.data();

        for (size_t i = 0; i < textures.size(); ++i) {
            const auto& texture = textures[i];

            std::vector<VkDescriptorSet> descriptorSets(maxFramesInFlight);

            if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) continue;

            // Prepare the write structures outside the loop
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = texture.view;
            imageInfo.sampler = texture.sampler;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &imageInfo;

            for (uint32_t j = 0; j < maxFramesInFlight; ++j) {
                descriptorWrite.dstSet = descriptorSets[j];

                vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
            }

            manySets.push_back(std::move(descriptorSets));
        }
    }



    DynamicDescriptor::~DynamicDescriptor() {
        for (auto& set : sets) vkFreeDescriptorSets(device, pool, 1, &set);

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

    void DynamicDescriptor::createPool(uint32_t maxResources, const std::vector<VkDescriptorType>& types) {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, pool, nullptr);
            pool = VK_NULL_HANDLE;
        }

        this->maxResources = maxResources;

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

    VkDescriptorSetLayoutBinding DynamicDescriptor::fastBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t descriptorCount) {
        VkDescriptorSetLayoutBinding bindingInfo{};
        bindingInfo.binding = binding;
        bindingInfo.descriptorCount = descriptorCount;
        bindingInfo.descriptorType = type;
        bindingInfo.pImmutableSamplers = nullptr;
        bindingInfo.stageFlags = stageFlags;
        return bindingInfo;
    }
}
