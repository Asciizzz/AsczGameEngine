#pragma once

#include "AzVulk/Pipeline_compute.hpp"
#include "AzVulk/DescriptorSets.hpp"
#include "AzVulk/Buffer.hpp"

#include <stdexcept>

namespace AzVulk {

class ComputeTask {
public:
    ComputeTask(const Device* device, const std::string& compShaderPath)
    : vkDevice(device), shaderPath(compShaderPath) {
        descriptor.init(vkDevice->device);
    }
    ComputeTask() = default;

    void init(const Device* device, const std::string& compShaderPath) {
        vkDevice = device;
        shaderPath = compShaderPath;
        descriptor.init(vkDevice->device);
    }

    // Add buffers to bind (storage/uniform)
    void addStorageBuffer(BufferData& buffer, uint32_t binding) {
        VkDescriptorSetLayoutBinding layout = DynamicDescriptor::fastBinding(
            binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
        );
        bindings.push_back(layout);

        buffers.push_back(&buffer);
    }

    void create() {
        // 1. Create descriptor set layout
        descriptor.createLayout(bindings);

        // 2. Create descriptor pool
        descriptor.createPool({
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(buffers.size())}
        }, 1);

        // 3. Allocate descriptor set
        VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorPool = descriptor.pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptor.setLayout;

        if (vkAllocateDescriptorSets(vkDevice->device, &allocInfo, &descSet) != VK_SUCCESS) {
            throw std::runtime_error("ComputeTask: failed to allocate descriptor set");
        }

        // 4. Update descriptor set with buffers

        for (size_t i = 0; i < buffers.size(); ++i) {
            VkDescriptorBufferInfo bufInfo{};
            bufInfo.buffer = buffers[i]->buffer;
            bufInfo.offset = 0;
            bufInfo.range  = VK_WHOLE_SIZE;

            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = descSet;
            write.dstBinding = bindings[i].binding;
            write.descriptorType = bindings[i].descriptorType;
            write.descriptorCount = 1;
            write.pBufferInfo = &bufInfo;

            vkUpdateDescriptorSets(vkDevice->device, 1, &write, 0, nullptr);
        }

        // vkUpdateDescriptorSets(vkDevice->device, (uint32_t)writes.size(), writes.data(), 0, nullptr);

        // 5. Build compute pipeline
        ComputePipelineConfig config;
        config.setLayouts = { descriptor.setLayout };
        config.compPath   = shaderPath;

        pipeline = std::make_unique<ComputePipeline>(vkDevice->device, config);
        pipeline->create();
    }

    void dispatch(uint32_t numElems, uint32_t groupSize = 64) {
        uint32_t numGroups = (numElems + groupSize - 1) / groupSize;

        TemporaryCommand tempCmd(vkDevice, "Default_Compute");

        vkCmdBindPipeline(tempCmd.getCmdBuffer(),
                          VK_PIPELINE_BIND_POINT_COMPUTE,
                          pipeline->pipeline);

        vkCmdBindDescriptorSets(tempCmd.getCmdBuffer(),
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                pipeline->layout,
                                0, 1, &descSet,
                                0, nullptr);

        vkCmdDispatch(tempCmd.getCmdBuffer(), numGroups, 1, 1);

        // Ensure results visible to host
        for (auto* buf : buffers) {
            VkBufferMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            barrier.buffer = buf->buffer;
            barrier.offset = 0;
            barrier.size   = VK_WHOLE_SIZE;

            vkCmdPipelineBarrier(tempCmd.getCmdBuffer(),
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_HOST_BIT,
                0, 0, nullptr, 1, &barrier, 0, nullptr);
        }

        tempCmd.endAndSubmit();
    }

private:
    const Device* vkDevice;
    std::string shaderPath;

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<BufferData*> buffers;

    DynamicDescriptor descriptor;
    VkDescriptorSet descSet{ VK_NULL_HANDLE };

    std::unique_ptr<ComputePipeline> pipeline;
};

}
