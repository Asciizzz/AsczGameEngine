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
        allocInfo.commandPool = vkDevice->computePoolWrapper.pool;
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

        // 5. Build compute pipeline
        ComputePipelineConfig config;
        config.setLayouts = { descriptor.setLayout };
        config.compPath   = shaderPath;

        pipeline = std::make_unique<ComputePipeline>(vkDevice->device, config);
        pipeline->create();
    }

    void ComputeTask::dispatchAsync(uint32_t numElems, uint32_t groupSize = 256, bool readBack = true) {
        if (!pipeline) throw std::runtime_error("ComputeTask: pipeline not created");

        // Calculate number of workgroups
        uint32_t numGroups = (numElems + groupSize - 1) / groupSize;

        // Begin recording
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdBuffer, &beginInfo);

        // Bind pipeline + descriptor
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->layout,
                                0, 1, &descSet, 0, nullptr);

        // Dispatch
        vkCmdDispatch(cmdBuffer, numGroups, 1, 1);

        // Memory barrier for device->host read
        if (readBack && !buffers.empty()) {
            VkBufferMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.buffer = buffers[0]->buffer; // assuming first buffer is the output
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;

            vkCmdPipelineBarrier(cmdBuffer,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                0, 0, nullptr,
                                1, &barrier,
                                0, nullptr);
        }

        vkEndCommandBuffer(cmdBuffer);

        // Submit without waiting
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        VkFence fence;
        VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        vkCreateFence(vkDevice->device, &fenceInfo, nullptr, &fence);

        vkQueueSubmit(vkDevice->computeQueue, 1, &submitInfo, fence);

        // Only wait if readback is needed
        if (readBack) {
            vkWaitForFences(vkDevice->device, 1, &fence, VK_TRUE, UINT64_MAX);

            // Copy to CPU via mapped staging buffer if necessary
            // (you can add a helper for this)
        }

        vkDestroyFence(vkDevice->device, fence, nullptr);
    }


    template<typename T>
    static void fetchResults(BufferData& buf, T* dst, VkDeviceSize size) {
        if (!buf.hostVisible) {
            // Create a host-visible staging buffer
            BufferData staging(buf.vkDevice);
            staging.setProperties(size,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
            staging.createBuffer();

            // Copy GPU buffer to staging
            TemporaryCommand copyCmd(buf.vkDevice, buf.vkDevice->computePoolWrapper);
            VkBufferCopy region{};
            region.size = size;
            vkCmdCopyBuffer(copyCmd.cmdBuffer, buf.buffer, staging.buffer, 1, &region);
            copyCmd.endAndSubmit();

            std::memcpy(dst, staging.mapped, size);
        } else {
            std::memcpy(dst, buf.mapped, size);
        }
    }





    // Helpers for creating buffers;
    template<typename T>
    static void makeStorageBuffer(BufferData& buf,
                                const T* src,
                                VkDeviceSize size,
                                bool deviceLocalOutput = false)
    {
        if (deviceLocalOutput) {
            // 1. Create staging buffer for upload
            BufferData staging(buf.vkDevice);
            staging.setProperties(
                size,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
            staging.createBuffer();
            staging.mappedData(src);

            // 2. Create device-local buffer for GPU compute
            buf.setProperties(
                size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            );
            buf.createBuffer();

            // 3. Submit copy from staging to device-local
            TemporaryCommand copyCmd(buf.vkDevice, buf.vkDevice->computePoolWrapper);
            VkBufferCopy region{};
            region.size = size;
            vkCmdCopyBuffer(copyCmd.cmdBuffer, staging.buffer, buf.buffer, 1, &region);

            VkBufferMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            barrier.buffer = buf.buffer;
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;

            vkCmdPipelineBarrier(copyCmd.cmdBuffer,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                0, 0, nullptr, 1, &barrier, 0, nullptr);

            copyCmd.endAndSubmit();

            buf.hostVisible = false; // mark as device-local
        } else {
            // fallback to host-visible buffer (CPU read/write)
            buf.setProperties(
                size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
            buf.createBuffer();
            buf.mappedData(src);
        }
    }


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
        TemporaryCommand copyCmd(deviceBuf.vkDevice, deviceBuf.vkDevice->computePoolWrapper);

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

    VkFence lastFence{ VK_NULL_HANDLE };

    std::unique_ptr<ComputePipeline> pipeline;
};

}
