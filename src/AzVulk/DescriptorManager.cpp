#include "AzVulk/DescriptorManager.hpp"
#include "AzVulk/Buffer.hpp"
#include "Az3D/Az3D.hpp"
#include <stdexcept>

namespace AzVulk {

    void DescriptorManager::freeAllDescriptorSets() {
        // Free material descriptor sets
        if (materialDescriptorPool != VK_NULL_HANDLE) {
            for (auto& pair : materialDescriptorSets) {
                const std::vector<VkDescriptorSet>& sets = pair.second;
                if (!sets.empty()) {
                    vkFreeDescriptorSets(vulkanDevice.device, materialDescriptorPool, static_cast<uint32_t>(sets.size()), sets.data());
                }
            }
            materialDescriptorSets.clear();
        }
        // Free global descriptor sets
        if (globalDescriptorPool != VK_NULL_HANDLE && !globalDescriptorSets.empty()) {
            vkFreeDescriptorSets(vulkanDevice.device, globalDescriptorPool, static_cast<uint32_t>(globalDescriptorSets.size()), globalDescriptorSets.data());
            globalDescriptorSets.clear();
        }
    }
    DescriptorManager::DescriptorManager(const Device& device)
        : vulkanDevice(device) {
    }

    DescriptorManager::~DescriptorManager() {
        // Free and destroy material descriptor sets and pool
        if (materialDescriptorPool != VK_NULL_HANDLE) {
            for (auto& pair : materialDescriptorSets) {
                const std::vector<VkDescriptorSet>& sets = pair.second;
                if (!sets.empty()) {
                    vkFreeDescriptorSets(vulkanDevice.device, materialDescriptorPool, static_cast<uint32_t>(sets.size()), sets.data());
                }
            }
            materialDescriptorSets.clear();
            vkDestroyDescriptorPool(vulkanDevice.device, materialDescriptorPool, nullptr);
        }
        // Free and destroy global descriptor sets and pool
        if (globalDescriptorPool != VK_NULL_HANDLE) {
            if (!globalDescriptorSets.empty()) {
                vkFreeDescriptorSets(vulkanDevice.device, globalDescriptorPool, static_cast<uint32_t>(globalDescriptorSets.size()), globalDescriptorSets.data());
            }
            globalDescriptorSets.clear();
            vkDestroyDescriptorPool(vulkanDevice.device, globalDescriptorPool, nullptr);
        }
        // Destroy layouts
        if (materialDescriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(vulkanDevice.device, materialDescriptorSetLayout, nullptr);
        }
        if (globalDescriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(vulkanDevice.device, globalDescriptorSetLayout, nullptr);
        }
    }

    // Create two descriptor set layouts: set 0 (global UBO), set 1 (material UBO + texture)
    void DescriptorManager::createDescriptorSetLayouts() {
        // Set 0: Global UBO
        if (globalDescriptorSetLayout == VK_NULL_HANDLE) {
            VkDescriptorSetLayoutBinding globalUBOBinding{};
            globalUBOBinding.binding = 0;
            globalUBOBinding.descriptorCount = 1;
            globalUBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            globalUBOBinding.pImmutableSamplers = nullptr;
            globalUBOBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

            VkDescriptorSetLayoutCreateInfo globalLayoutInfo{};
            globalLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            globalLayoutInfo.bindingCount = 1;
            globalLayoutInfo.pBindings = &globalUBOBinding;

            if (vkCreateDescriptorSetLayout(vulkanDevice.device, &globalLayoutInfo, nullptr, &globalDescriptorSetLayout) != VK_SUCCESS) {
                throw std::runtime_error("failed to create global descriptor set layout!");
            }
        }

        // Set 1: Material UBO + Texture
        if (materialDescriptorSetLayout == VK_NULL_HANDLE) {
            VkDescriptorSetLayoutBinding materialUBOBinding{};
            materialUBOBinding.binding = 0;
            materialUBOBinding.descriptorCount = 1;
            materialUBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            materialUBOBinding.pImmutableSamplers = nullptr;
            materialUBOBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

            VkDescriptorSetLayoutBinding textureBinding{};
            textureBinding.binding = 1;
            textureBinding.descriptorCount = 1;
            textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            textureBinding.pImmutableSamplers = nullptr;
            textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

            std::array<VkDescriptorSetLayoutBinding, 2> bindings = {materialUBOBinding, textureBinding};

            VkDescriptorSetLayoutCreateInfo materialLayoutInfo{};
            materialLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            materialLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
            materialLayoutInfo.pBindings = bindings.data();

            if (vkCreateDescriptorSetLayout(vulkanDevice.device, &materialLayoutInfo, nullptr, &materialDescriptorSetLayout) != VK_SUCCESS) {
                throw std::runtime_error("failed to create material descriptor set layout!");
            }
        }
    }

    // Create two pools: one for global UBOs, one for material sets
    void DescriptorManager::createDescriptorPools(uint32_t maxFramesInFlight, uint32_t maxMaterials) {
        // Destroy old pools if they exist
        if (globalDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(vulkanDevice.device, globalDescriptorPool, nullptr);
            globalDescriptorPool = VK_NULL_HANDLE;
        }
        if (materialDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(vulkanDevice.device, materialDescriptorPool, nullptr);
            materialDescriptorPool = VK_NULL_HANDLE;
        }

        this->maxFramesInFlight = maxFramesInFlight;
        this->maxMaterials = maxMaterials;

        // Pool for global UBOs (one per frame)
        VkDescriptorPoolSize globalPoolSize{};
        globalPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        globalPoolSize.descriptorCount = maxFramesInFlight;

        VkDescriptorPoolCreateInfo globalPoolInfo{};
        globalPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        globalPoolInfo.poolSizeCount = 1;
        globalPoolInfo.pPoolSizes = &globalPoolSize;
        globalPoolInfo.maxSets = maxFramesInFlight;
        globalPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        if (vkCreateDescriptorPool(vulkanDevice.device, &globalPoolInfo, nullptr, &globalDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create global descriptor pool!");
        }

        // Pool for material sets (material UBO + texture, per material per frame)
        std::array<VkDescriptorPoolSize, 2> materialPoolSizes{};
        materialPoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        materialPoolSizes[0].descriptorCount = maxMaterials * maxFramesInFlight;
        materialPoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        materialPoolSizes[1].descriptorCount = maxMaterials * maxFramesInFlight;

        VkDescriptorPoolCreateInfo materialPoolInfo{};
        materialPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        materialPoolInfo.poolSizeCount = static_cast<uint32_t>(materialPoolSizes.size());
        materialPoolInfo.pPoolSizes = materialPoolSizes.data();
        materialPoolInfo.maxSets = maxMaterials * maxFramesInFlight;
        materialPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        if (vkCreateDescriptorPool(vulkanDevice.device, &materialPoolInfo, nullptr, &materialDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create material descriptor pool!");
        }
    }


    // Create global UBO descriptor sets (set 0, one per frame)
    void DescriptorManager::createGlobalDescriptorSets(const std::vector<VkBuffer>& uniformBuffers, size_t uniformBufferSize) {
        std::vector<VkDescriptorSetLayout> layouts(maxFramesInFlight, globalDescriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = globalDescriptorPool;
        allocInfo.descriptorSetCount = maxFramesInFlight;
        allocInfo.pSetLayouts = layouts.data();

        globalDescriptorSets.resize(maxFramesInFlight);
        if (vkAllocateDescriptorSets(vulkanDevice.device, &allocInfo, globalDescriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate global descriptor sets");
        }

        for (uint32_t i = 0; i < maxFramesInFlight; ++i) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = uniformBufferSize;

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = globalDescriptorSets[i];
            write.dstBinding = 0;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(vulkanDevice.device, 1, &write, 0, nullptr);
        }
    }

    // Create material descriptor sets (set 1, per material per frame)
    void DescriptorManager::createMaterialDescriptorSets(const Az3D::Texture* texture, VkBuffer materialUniformBuffer, size_t materialIndex) {
        std::vector<VkDescriptorSetLayout> layouts(maxFramesInFlight, materialDescriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = materialDescriptorPool;
        allocInfo.descriptorSetCount = maxFramesInFlight;
        allocInfo.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> descriptorSets(maxFramesInFlight);
        if (vkAllocateDescriptorSets(vulkanDevice.device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
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

            vkUpdateDescriptorSets(vulkanDevice.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }

        materialDescriptorSets[materialIndex] = std::move(descriptorSets);
    }

    VkDescriptorSet DescriptorManager::getMaterialDescriptorSet(uint32_t frameIndex, size_t materialIndex) {
        auto it = materialDescriptorSets.find(materialIndex);
        if (it != materialDescriptorSets.end() && frameIndex < it->second.size()) {
            return it->second[frameIndex];
        }
        throw std::runtime_error("Material descriptor set not found for material index: " + std::to_string(materialIndex) + " frame: " + std::to_string(frameIndex));
    }

    VkDescriptorSet DescriptorManager::getGlobalDescriptorSet(uint32_t frameIndex) {
        if (frameIndex < globalDescriptorSets.size()) {
            return globalDescriptorSets[frameIndex];
        }
        throw std::runtime_error("Global descriptor set not found for frame: " + std::to_string(frameIndex));
    }




// DYNAMIC DESCRIPTOR SETS

    DynamicDescriptor::~DynamicDescriptor() {
        for (auto& set : sets) {
            vkFreeDescriptorSets(device, pool, 1, &set);
        }

        if (setLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
        if (pool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, pool, nullptr);
    }

    void DynamicDescriptor::createDynamicDescriptor(
        VkDevice device, uint32_t maxResources, uint32_t maxFramesInFlight,
        const std::vector<VkDescriptorSetLayoutBinding>& bindings,
        const std::vector<VkDescriptorType>& types,
        std::vector<VkWriteDescriptorSet>& writes) {

        this->device = device;
        this->maxResources = maxResources;
        this->maxFramesInFlight = maxFramesInFlight;

        createSetLayout(bindings);
        createPool(types);
        createDescriptorSet(writes);
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

    void DynamicDescriptor::createDescriptorSet(std::vector<VkWriteDescriptorSet>& writes) {
        std::vector<VkDescriptorSetLayout> layouts(maxResources, setLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocInfo.pSetLayouts = layouts.data();

        sets.resize(maxResources);
        if (vkAllocateDescriptorSets(device, &allocInfo, sets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets");
        }

        for (uint32_t i = 0; i < maxFramesInFlight; ++i) {
            for (auto& write : writes) write.dstSet = sets[i];

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }
}
