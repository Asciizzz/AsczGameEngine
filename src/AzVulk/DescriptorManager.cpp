#include "AzVulk/DescriptorManager.hpp"
#include "AzVulk/Buffer.hpp"
#include "Az3D/Az3D.hpp"
#include <stdexcept>

namespace AzVulk {

    void DescriptorManager::freeAllDescriptorSets() {
        if (descriptorPool != VK_NULL_HANDLE) {
            for (auto& pair : materialDescriptorSets) {
                const std::vector<VkDescriptorSet>& sets = pair.second;
                if (!sets.empty()) {
                    vkFreeDescriptorSets(vulkanDevice.device, descriptorPool, static_cast<uint32_t>(sets.size()), sets.data());
                }
            }
            materialDescriptorSets.clear();
        }
    }
    DescriptorManager::DescriptorManager(const Device& device)
        : vulkanDevice(device) {
    }

    DescriptorManager::~DescriptorManager() {
        // Explicitly free all descriptor sets if pool allows it
        if (descriptorPool != VK_NULL_HANDLE) {
            for (auto& pair : materialDescriptorSets) {
                const std::vector<VkDescriptorSet>& sets = pair.second;
                if (!sets.empty()) {
                    vkFreeDescriptorSets(vulkanDevice.device, descriptorPool, static_cast<uint32_t>(sets.size()), sets.data());
                }
            }
            materialDescriptorSets.clear();
            vkDestroyDescriptorPool(vulkanDevice.device, descriptorPool, nullptr);
        }
        if (descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(vulkanDevice.device, descriptorSetLayout, nullptr);
        }
    }

    VkDescriptorSetLayout DescriptorManager::createStandardRasterLayout() {
        if (descriptorSetLayout != VK_NULL_HANDLE) {
            return descriptorSetLayout; // Already created
        }

        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.pImmutableSamplers = nullptr;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding textureLayoutBinding{};
        textureLayoutBinding.binding = 1;
        textureLayoutBinding.descriptorCount = 1;
        textureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        textureLayoutBinding.pImmutableSamplers = nullptr;
        textureLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding materialLayoutBinding{};
        materialLayoutBinding.binding = 2;
        materialLayoutBinding.descriptorCount = 1;
        materialLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        materialLayoutBinding.pImmutableSamplers = nullptr;
        materialLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;


    std::array<VkDescriptorSetLayoutBinding, 3> bindings = {uboLayoutBinding, textureLayoutBinding, materialLayoutBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(vulkanDevice.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }

        return descriptorSetLayout;
    }

    void DescriptorManager::createDescriptorPool(uint32_t maxFramesInFlight, uint32_t maxMaterials) {
        // Destroy old pool if it exists (after freeing sets)
        if (descriptorPool != VK_NULL_HANDLE) {
            freeAllDescriptorSets();
            vkDestroyDescriptorPool(vulkanDevice.device, descriptorPool, nullptr);
            descriptorPool = VK_NULL_HANDLE;
        }

        this->maxFramesInFlight = maxFramesInFlight;
        this->maxMaterials = maxMaterials;

        std::array<VkDescriptorPoolSize, 3> poolSizes{};

        // Global uniform buffers: maxMaterials * maxFramesInFlight (each material needs uniform buffer per frame)
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = maxMaterials * maxFramesInFlight;

        // Combined image samplers: maxMaterials * maxFramesInFlight (only material textures, no depth)
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = maxMaterials * maxFramesInFlight;

        // Material uniform buffers: maxMaterials (one per material, not per frame)
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[2].descriptorCount = maxMaterials;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = maxMaterials * maxFramesInFlight; // Total descriptor sets
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; // Allow freeing individual sets

        if (vkCreateDescriptorPool(vulkanDevice.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void DescriptorManager::createDescriptorSets(const std::vector<VkBuffer>& uniformBuffers, size_t uniformBufferSize,
                                                const Az3D::Texture* texture, VkBuffer materialUniformBuffer,
                                                size_t materialIndex) {
        std::vector<VkDescriptorSetLayout> layouts(maxFramesInFlight, descriptorSetLayout);
        
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = maxFramesInFlight;
        allocInfo.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> descriptorSets(maxFramesInFlight);
        if (vkAllocateDescriptorSets(vulkanDevice.device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets for material index: " + std::to_string(materialIndex));
        }

        // Configure each descriptor set for this material
        // I have no regard for safety, fear me
        for (uint32_t i = 0; i < maxFramesInFlight; ++i) {
            std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

            // Global uniform buffer binding (binding 0)
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = uniformBufferSize;

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = descriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;

            // Texture binding (binding 1)
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = texture->view;
            imageInfo.sampler = texture->sampler;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = descriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo;

            // Material uniform buffer binding (binding 2)
            VkDescriptorBufferInfo materialBufferInfo{};
            materialBufferInfo.buffer = materialUniformBuffer;
            materialBufferInfo.offset = 0;
            materialBufferInfo.range = sizeof(MaterialUBO);

            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = descriptorSets[i];
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pBufferInfo = &materialBufferInfo;

            // Write all descriptors (global UBO, texture, material UBO)
            uint32_t writeCount = 3;
            vkUpdateDescriptorSets(vulkanDevice.device, writeCount, descriptorWrites.data(), 0, nullptr);
        }

        // Store the descriptor sets for this material
        materialDescriptorSets[materialIndex] = std::move(descriptorSets);
    }

    VkDescriptorSet DescriptorManager::getDescriptorSet(uint32_t frameIndex, size_t materialIndex) {
        auto it = materialDescriptorSets.find(materialIndex);
        if (it != materialDescriptorSets.end() && frameIndex < it->second.size()) {
            return it->second[frameIndex];
        }
        
        throw std::runtime_error("Descriptor set not found for material index: " + std::to_string(materialIndex) + " frame: " + std::to_string(frameIndex));
    }
}
