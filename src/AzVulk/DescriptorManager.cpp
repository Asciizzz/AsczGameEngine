#include "AzVulk/DescriptorManager.hpp"
#include "Az3D/Az3D.hpp"
#include <stdexcept>
#include <array>

namespace AzVulk {
    DescriptorManager::DescriptorManager(const VulkanDevice& device, VkDescriptorSetLayout descriptorSetLayout)
        : vulkanDevice(device), descriptorSetLayout(descriptorSetLayout) {
    }

    DescriptorManager::~DescriptorManager() {
        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(vulkanDevice.device, descriptorPool, nullptr);
        }
    }

    void DescriptorManager::createDescriptorPool(uint32_t maxFramesInFlight, uint32_t maxMaterials) {
        this->maxFramesInFlight = maxFramesInFlight;
        this->maxMaterials = maxMaterials;
        
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        
        // Uniform buffers: maxMaterials * maxFramesInFlight (each material needs uniform buffer per frame)
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = maxMaterials * maxFramesInFlight;
        
        // Combined image samplers: maxMaterials * maxFramesInFlight 
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = maxMaterials * maxFramesInFlight;

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

    void DescriptorManager::createDescriptorSetsForMaterial(const std::vector<VkBuffer>& uniformBuffers, size_t uniformBufferSize, 
                                                           const Az3D::Texture* texture, const std::string& materialId) {
        // Check if material already has descriptor sets
        if (materialDescriptorSets.find(materialId) != materialDescriptorSets.end()) {
            return; // Already created
        }

        std::vector<VkDescriptorSetLayout> layouts(maxFramesInFlight, descriptorSetLayout);
        
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = maxFramesInFlight;
        allocInfo.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> descriptorSets(maxFramesInFlight);
        if (vkAllocateDescriptorSets(vulkanDevice.device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets for material: " + materialId);
        }

        // Configure each descriptor set for this material
        for (uint32_t i = 0; i < maxFramesInFlight; i++) {
            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

            // Uniform buffer binding
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

            // Texture binding
            VkDescriptorImageInfo imageInfo{};
            bool hasValidTexture = false;
            
            if (texture && texture->getData()) {
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = texture->getData()->view;
                imageInfo.sampler = texture->getData()->sampler;
                hasValidTexture = true;
            }

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = descriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo;

            // Write both uniform buffer and texture (if texture is valid)
            uint32_t writeCount = hasValidTexture ? 2 : 1;
            vkUpdateDescriptorSets(vulkanDevice.device, writeCount, descriptorWrites.data(), 0, nullptr);
        }

        // Store the descriptor sets for this material
        materialDescriptorSets[materialId] = std::move(descriptorSets);
    }

    VkDescriptorSet DescriptorManager::getDescriptorSet(uint32_t frameIndex, const std::string& materialId) {
        auto it = materialDescriptorSets.find(materialId);
        if (it != materialDescriptorSets.end() && frameIndex < it->second.size()) {
            return it->second[frameIndex];
        }
        
        throw std::runtime_error("Descriptor set not found for material: " + materialId + " frame: " + std::to_string(frameIndex));
    }
}
