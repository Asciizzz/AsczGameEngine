#include "AzVulk/DescriptorManager.hpp"
#include "AzVulk/Buffer.hpp"
#include "Az3D/Az3D.hpp"
#include <stdexcept>

namespace AzVulk {

    DescriptorManager::DescriptorManager(VkDevice device) : device(device) {}

    DescriptorManager::~DescriptorManager() {}


    void DescriptorManager::createDescriptorSetLayouts(uint32_t maxFramesInFlight) {
        // For global ubo + depth sampler
        globalDynamicDescriptor.init(device);
        VkDescriptorSetLayoutBinding globalUBOBinding = DynamicDescriptor::fastBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        VkDescriptorSetLayoutBinding depthSamplerBinding = DynamicDescriptor::fastBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        globalDynamicDescriptor.createSetLayout({globalUBOBinding, depthSamplerBinding});
    }

    void DescriptorManager::createDescriptorPools(uint32_t maxFramesInFlight) {
        globalDynamicDescriptor.createPool(
            {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxFramesInFlight},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxFramesInFlight}},
            maxFramesInFlight
        );
    }


    void DynamicDescriptor::createGlobalDescriptorSets(
        const std::vector<BufferData>& uniformBufferDatas,
        size_t uniformBufferSize,
        VkImageView depthImageView,
        VkSampler depthSampler, uint32_t maxFramesInFlight
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
            bufferInfo.buffer = uniformBufferDatas[i].buffer;
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

    void DynamicDescriptor::createPool(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets) {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, pool, nullptr);
            pool = VK_NULL_HANDLE;
        }

        this->poolSizes = poolSizes;


        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = maxSets;
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
