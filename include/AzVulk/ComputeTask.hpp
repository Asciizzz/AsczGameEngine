#pragma once

#include "AzVulk/Pipeline_compute.hpp"
#include "AzVulk/DescriptorSets.hpp"
#include "AzVulk/Buffer.hpp"

#include <stdexcept>

namespace AzVulk {

class ComputeTask {
public:
    ComputeTask(const Device* device, const std::string& compShaderPath) {
        init(device, compShaderPath);
    }
    ComputeTask() = default;

    void init(const Device* device, const std::string& compShaderPath) {
        vkDevice = device;
        shaderPath = compShaderPath;
        descriptor.init(vkDevice->device);

        // Create command buffer
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = vkDevice->getCommandPool("Default_Compute");
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(vkDevice->device, &allocInfo, &cmdBuffer) != VK_SUCCESS) {
            throw std::runtime_error("ComputeTask: failed to allocate command buffer");
        }
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

    void dispatch(uint32_t numElems, uint32_t groupSize = 128) { // increased group size for better occupancy
        if (!pipeline) throw std::runtime_error("ComputeTask: pipeline not created");

        uint32_t numGroups = (numElems + groupSize - 1) / groupSize;

        // Begin recording command buffer (reuse pre-allocated cmdBuffer)
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmdBuffer, &beginInfo);

        // Bind pipeline and descriptor set
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);
        vkCmdBindDescriptorSets(cmdBuffer,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                pipeline->layout,
                                0, 1, &descSet,
                                0, nullptr);

        // Dispatch compute shader
        vkCmdDispatch(cmdBuffer, numGroups, 1, 1);

        // Memory barrier to ensure GPU writes are visible to host (batch all buffers)
        if (!buffers.empty()) {
            std::vector<VkBufferMemoryBarrier> barriers(buffers.size());
            for (size_t i = 0; i < buffers.size(); ++i) {
                barriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                barriers[i].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barriers[i].dstAccessMask = VK_ACCESS_HOST_READ_BIT;
                barriers[i].buffer = buffers[i]->buffer;
                barriers[i].offset = 0;
                barriers[i].size = VK_WHOLE_SIZE;
            }

            vkCmdPipelineBarrier(cmdBuffer,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                VK_PIPELINE_STAGE_HOST_BIT,
                                0, 0, nullptr,
                                static_cast<uint32_t>(barriers.size()), barriers.data(),
                                0, nullptr);
        }

        vkEndCommandBuffer(cmdBuffer);

        // Submit command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkFence fence;
        vkCreateFence(vkDevice->device, &fenceInfo, nullptr, &fence);

        vkQueueSubmit(vkDevice->computeQueue, 1, &submitInfo, fence);

        // Wait for completion (optional: remove to allow async execution)
        vkWaitForFences(vkDevice->device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(vkDevice->device, fence, nullptr);
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
    static void uploadDeviceStorageBuffer(BufferData& deviceBuf, const T* srcData, VkDeviceSize size) {
        // 1. Create staging buffer
        BufferData staging(deviceBuf.vkDevice);
        staging.setProperties(
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        staging.createBuffer();
        staging.mappedData(srcData);

        // 2. Create device-local buffer
        deviceBuf.setProperties(
            size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        deviceBuf.createBuffer();

        // 3. Submit single copy command
        TemporaryCommand copyCmd(deviceBuf.vkDevice, "Default_Transfer");

        VkBufferCopy region{};
        region.srcOffset = 0;
        region.dstOffset = 0;
        region.size = size;

        vkCmdCopyBuffer(copyCmd.cmdBuffer, staging.buffer, deviceBuf.buffer, 1, &region);

        // Barrier to ensure copy completes before GPU compute
        VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        barrier.buffer = deviceBuf.buffer;
        barrier.offset = 0;
        barrier.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(copyCmd.cmdBuffer,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 0, nullptr,
                            1, &barrier,
                            0, nullptr);

        copyCmd.endAndSubmit();  // ensures staging buffer lives until GPU finished

        deviceBuf.hostVisible = false;
    }


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

    VkCommandBuffer cmdBuffer;

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<BufferData*> buffers;

    DynamicDescriptor descriptor;
    VkDescriptorSet descSet{ VK_NULL_HANDLE };

    std::unique_ptr<ComputePipeline> pipeline;
};

}
