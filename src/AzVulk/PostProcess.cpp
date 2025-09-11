#include "AzVulk/PostProcess.hpp"

#include <stdexcept>
#include <iostream>

using namespace AzVulk;

PostEffect::PostEffect(const Device* vkDevice, const std::string& computeShaderPath)
: vkDevice(vkDevice), shaderPath(computeShaderPath)
{
    loadShader();
    createDescriptorLayout();
}

PostEffect::~PostEffect() {
    if (shaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(vkDevice->lDevice, shaderModule, nullptr);
    }
}

void PostEffect::loadShader() {
    // Load shader code using existing PipelineCore utility
    auto code = PipelineCore::readFile(shaderPath);
    
    // Create shader module using existing PipelineCore utility
    PipelineCore core(vkDevice->lDevice);
    shaderModule = core.createModule(code);
    
    // Set up shader stage info
    shaderStageInfo = {};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";
}

void PostEffect::createDescriptorLayout() {
    descLayout.init(vkDevice->lDevice);
    
    // Standard post-processing descriptor layout:
    // Binding 0: Input image (previous pass result or swapchain image)
    // Binding 1: Output image (target for this pass)
    descLayout.create({
        DescLayout::BindInfo{0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT}, // Input
        DescLayout::BindInfo{1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT}  // Output
    });
}

void PostEffect::updateDescriptorSet(VkDescriptorSet descriptorSet, VkImageView inputImage, VkImageView outputImage) {
    VkDescriptorImageInfo inputImageInfo{};
    inputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    inputImageInfo.imageView = inputImage;
    inputImageInfo.sampler = VK_NULL_HANDLE; // Storage images don't use samplers

    VkDescriptorImageInfo outputImageInfo{};
    outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    outputImageInfo.imageView = outputImage;
    outputImageInfo.sampler = VK_NULL_HANDLE;

    std::vector<VkWriteDescriptorSet> writes(2);
    
    // Input image
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descriptorSet;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &inputImageInfo;

    // Output image
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descriptorSet;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &outputImageInfo;

    vkUpdateDescriptorSets(vkDevice->lDevice, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

PostProcessor::PostProcessor(const Device* vkDevice, const SwapChain* swapChain)
: vkDevice(vkDevice), swapChain(swapChain), currentFrameIndex(0)
{
    createPingPongImages();
    createDescriptorPool();
    createCommandBuffers();
    createPipelines();
}

PostProcessor::~PostProcessor() {
    cleanup();
}

void PostProcessor::cleanup() {
    // Wait for device to be idle before cleanup
    vkDeviceWaitIdle(vkDevice->lDevice);
    
    // Cleanup pipelines and layouts
    for (size_t i = 0; i < pipelines.size(); ++i) {
        if (pipelines[i] != VK_NULL_HANDLE) {
            vkDestroyPipeline(vkDevice->lDevice, pipelines[i], nullptr);
        }
        if (pipelineLayouts[i] != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(vkDevice->lDevice, pipelineLayouts[i], nullptr);
        }
    }
    
    // CmdBuffer cleans up itself automatically
    
    // Delete effects (they will clean up their own shader modules)
    for (auto& effect : effects) {
        delete effect;
    }
    effects.clear();

    // Cleanup ping-pong images
    for (auto& image : pingPongImages) {
        if (image.imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(vkDevice->lDevice, image.imageView, nullptr);
        }
        if (image.image != VK_NULL_HANDLE) {
            vkDestroyImage(vkDevice->lDevice, image.image, nullptr);
        }
        if (image.memory != VK_NULL_HANDLE) {
            vkFreeMemory(vkDevice->lDevice, image.memory, nullptr);
        }
    }
}

void PostProcessor::addEffect(const std::string& computeShaderPath) {
    PostEffect* effect = new PostEffect(vkDevice, computeShaderPath);
    effects.push_back(effect);
    
    // Create descriptor sets for this effect (one per frame in flight)
    for (int frameIndex = 0; frameIndex < MAX_FRAMES_IN_FLIGHT; ++frameIndex) {
        DescSets descSet;
        descSet.init(vkDevice->lDevice);
        descSet.allocate(descPool.pool, effect->descLayout.layout, 1);
        effectDescriptorSets[effects.size() - 1][frameIndex] = std::move(descSet);
    }
    
    std::cout << "Added post-processing effect: " << computeShaderPath << std::endl;
}

void PostProcessor::createPingPongImages() {
    pingPongImages.resize(2);
    
    VkExtent3D extent = {
        swapChain->extent.width,
        swapChain->extent.height,
        1
    };
    
    for (int i = 0; i < 2; ++i) {
        // Create image
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = extent;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = swapChain->imageFormat; // Same format as swapchain
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(vkDevice->lDevice, &imageInfo, nullptr, &pingPongImages[i].image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create ping-pong image!");
        }

        // Allocate memory
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(vkDevice->lDevice, pingPongImages[i].image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(vkDevice->lDevice, &allocInfo, nullptr, &pingPongImages[i].memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate ping-pong image memory!");
        }

        vkBindImageMemory(vkDevice->lDevice, pingPongImages[i].image, pingPongImages[i].memory, 0);

        // Create image view
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = pingPongImages[i].image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapChain->imageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(vkDevice->lDevice, &viewInfo, nullptr, &pingPongImages[i].imageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create ping-pong image view!");
        }
    }
}

void PostProcessor::createDescriptorPool() {
    descPool.init(vkDevice->lDevice);
    
    // Estimate pool size: each effect needs 2 images * frames in flight
    uint32_t maxSets = MAX_EFFECTS * MAX_FRAMES_IN_FLIGHT;
    uint32_t imageDescriptors = MAX_EFFECTS * 2 * MAX_FRAMES_IN_FLIGHT; // 2 images per effect
    
    descPool.create({
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageDescriptors}
    }, maxSets);
}

void PostProcessor::createCommandBuffers() {
    cmdBuffer.create(vkDevice->lDevice, vkDevice->computePoolWrapper.pool, MAX_FRAMES_IN_FLIGHT);
}

void PostProcessor::createPipelines() {
    // Pipelines will be created when effects are added
    pipelines.resize(MAX_EFFECTS, VK_NULL_HANDLE);
    pipelineLayouts.resize(MAX_EFFECTS, VK_NULL_HANDLE);
}

void PostProcessor::execute(VkImageView inputImage, VkImageView outputImage, uint32_t frameIndex) {
    if (effects.empty()) {
        // No effects to apply, just copy input to output if needed
        return;
    }

    currentFrameIndex = frameIndex;
    VkCommandBuffer cmd = cmdBuffer[frameIndex];

    // Begin recording
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImageView currentInput = inputImage;
    VkImageView currentOutput = pingPongImages[0].imageView;
    
    for (size_t i = 0; i < effects.size(); ++i) {
        // Determine output for this pass
        if (i == effects.size() - 1) {
            // Last effect writes to final output
            currentOutput = outputImage;
        } else {
            // Ping-pong between the two intermediate images
            currentOutput = pingPongImages[(i + 1) % 2].imageView;
        }

        executeEffect(cmd, i, currentInput, currentOutput);
        
        // Next pass input is current pass output
        currentInput = currentOutput;
    }

    vkEndCommandBuffer(cmd);

    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    
    if (vkQueueSubmit(vkDevice->computeQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit compute command buffer!");
    }
    
    vkQueueWaitIdle(vkDevice->computeQueue); // TODO: Use proper synchronization
}

void PostProcessor::executeEffect(VkCommandBuffer cmd, size_t effectIndex, VkImageView inputImage, VkImageView outputImage) {
    if (effectIndex >= effects.size()) return;

    PostEffect* effect = effects[effectIndex];
    
    // Create pipeline for this effect if it doesn't exist
    if (pipelines[effectIndex] == VK_NULL_HANDLE) {
        createPipelineForEffect(effectIndex);
    }

    // Update descriptor set for this frame and effect
    VkDescriptorSet descriptorSet = effectDescriptorSets[effectIndex][currentFrameIndex].get();
    effect->updateDescriptorSet(descriptorSet, inputImage, outputImage);

    // Bind pipeline and descriptor set
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[effectIndex]);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayouts[effectIndex], 0, 1, &descriptorSet, 0, nullptr);

    // Dispatch compute shader
    uint32_t groupCountX = (swapChain->extent.width + 15) / 16;  // Assuming 16x16 workgroup size
    uint32_t groupCountY = (swapChain->extent.height + 15) / 16;
    vkCmdDispatch(cmd, groupCountX, groupCountY, 1);

    // Add memory barrier
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void PostProcessor::createPipelineForEffect(size_t effectIndex) {
    if (effectIndex >= effects.size()) return;

    PostEffect* effect = effects[effectIndex];

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &effect->descLayout.layout;

    if (vkCreatePipelineLayout(vkDevice->lDevice, &pipelineLayoutInfo, nullptr, &pipelineLayouts[effectIndex]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline layout!");
    }

    // Create compute pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = effect->shaderStageInfo;
    pipelineInfo.layout = pipelineLayouts[effectIndex];

    if (vkCreateComputePipelines(vkDevice->lDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelines[effectIndex]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline!");
    }
}

uint32_t PostProcessor::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(vkDevice->pDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}
