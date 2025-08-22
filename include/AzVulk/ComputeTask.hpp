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
    void addUniformBuffer(BufferData& buffer, uint32_t binding) {
        VkDescriptorSetLayoutBinding layout = DynamicDescriptor::fastBinding(
            binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
        );
        bindings.push_back(layout);

        buffers.push_back(&buffer);
    }
    
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
        uint32_t storageCount = 0, uniformCount = 0;
        for (const auto& b : bindings) {
            if (b.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) ++storageCount;
            if (b.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) ++uniformCount;
        }
        std::vector<VkDescriptorPoolSize> poolSizes;
        if (storageCount) poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, storageCount});
        if (uniformCount) poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniformCount});
        descriptor.createPool(poolSizes, 1);

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

        vkCmdBindPipeline(  tempCmd.cmdBuffer,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipeline->pipeline);

        vkCmdBindDescriptorSets(tempCmd.cmdBuffer,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                pipeline->layout,
                                0, 1, &descSet,
                                0, nullptr);

        vkCmdDispatch(tempCmd.cmdBuffer, numGroups, 1, 1);

        // Ensure results visible to host
        std::vector<VkBufferMemoryBarrier> barriers;
        for (auto* buf : buffers) {
            VkBufferMemoryBarrier b{};
            b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            b.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            b.buffer = buf->buffer;
            b.offset = 0;
            b.size = VK_WHOLE_SIZE;
            barriers.push_back(b);
        }
        vkCmdPipelineBarrier(tempCmd.cmdBuffer,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_HOST_BIT,
                            0, 0, nullptr,
                            static_cast<uint32_t>(barriers.size()), barriers.data(),
                            0, nullptr);

        tempCmd.endAndSubmit();
    }

    // Helpers for creating buffers;
    template<typename T>
    static void makeStorageBuffer(BufferData& buf, const T* src, VkDeviceSize size) {
        buf.setProperties(
            size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        buf.createBuffer();
        buf.mappedData(src);
    };

    template<typename T>
    static void makeDeviceStorageBuffer(BufferData& buf, const T* src, VkDeviceSize size) {
        // buf.setProperties(
        //     size,
        //     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        //     VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        //     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        //     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        //     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        // );
        // buf.createBuffer();
        // buf.mappedData(src);

        BufferData stagingBuffer(buf.vkDevice);
        stagingBuffer.setProperties(
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        stagingBuffer.createBuffer();
        stagingBuffer.mappedData(src);

        buf.setProperties(
            size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        buf.createBuffer();

        TemporaryCommand copyCmd(buf.vkDevice, "Default_Transfer");

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;

        vkCmdCopyBuffer(copyCmd.cmdBuffer, stagingBuffer.buffer, buf.buffer, 1, &copyRegion);
    };

    template<typename T>
    static void makeUniformBuffer(BufferData& buf, const T* src, VkDeviceSize size) {
        buf.setProperties(
            size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        buf.createBuffer();
        buf.mappedData(src);
    };

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
