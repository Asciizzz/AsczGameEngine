#include "AzVulk/PostProcess.hpp"
#include <stdexcept>
#include <iostream>

using namespace AzVulk;

// Destroy helper

void destroyImages(VkDevice lDevice, VkImage image) {
    if (image != VK_NULL_HANDLE) {
        vkDestroyImage(lDevice, image, nullptr);
        image = VK_NULL_HANDLE;
    }
}

void destroyImageViews(VkDevice lDevice, VkImageView imageView) {
    if (imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(lDevice, imageView, nullptr);
        imageView = VK_NULL_HANDLE;
    }
}

void destroyDeviceMemory(VkDevice lDevice, VkDeviceMemory memory) {
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(lDevice, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

void PingPongImages::cleanup(VkDevice device) {
    destroyImageViews(device, viewA);
    destroyImageViews(device, viewB);
    destroyImages(device, imageA);
    destroyImages(device, imageB);
    destroyDeviceMemory(device, memoryA);
    destroyDeviceMemory(device, memoryB);
}

void PostProcessEffect::cleanup(VkDevice device) {
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
    if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (pipeline) {
        pipeline->cleanup();
        pipeline.reset();
    }
}

PostProcess::PostProcess(Device* vkDevice, SwapChain* swapChain, DepthManager* depthManager)
    : vkDevice(vkDevice), swapChain(swapChain), depthManager(depthManager) {
}

PostProcess::~PostProcess() {
    cleanup();
}

void PostProcess::initialize() {
    createPingPongImages();
    createOffscreenRenderPass();
    createOffscreenFramebuffers();
    createSampler();
    createFinalBlit();
}

void PostProcess::createPingPongImages() {
    // Use R8G8B8A8_UNORM format which supports storage images
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    VkExtent2D extent = swapChain->extent;
    
    for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
        auto& images = pingPongImages[frame];
        
        // Create image A
        createImage(extent.width, extent.height, format,
                   VK_IMAGE_TILING_OPTIMAL,
                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | 
                   VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | 
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                   images.imageA, images.memoryA);
        
        // Create image B  
        createImage(extent.width, extent.height, format,
                   VK_IMAGE_TILING_OPTIMAL,
                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | 
                   VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | 
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                   images.imageB, images.memoryB);
        
        // Create image views
        images.viewA = createImageView(images.imageA, format, VK_IMAGE_ASPECT_COLOR_BIT);
        images.viewB = createImageView(images.imageB, format, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}



void PostProcess::createOffscreenRenderPass() {
    // Color attachment (ping-pong image A) - use storage-compatible format
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth attachment - using DepthManager's format
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthManager->depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 1; // Color is second attachment (depth first)
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 0; // Depth is first attachment 
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {depthAttachment, colorAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(vkDevice->lDevice, &renderPassInfo, nullptr, &offscreenRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create offscreen render pass!");
    }
}

void PostProcess::createOffscreenFramebuffers() {
    for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
        std::array<VkImageView, 2> attachments = {
            depthManager->depthImageView,  // Use DepthManager's depth buffer
            pingPongImages[frame].viewA  // Use image A as the initial render target
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = offscreenRenderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChain->extent.width;
        framebufferInfo.height = swapChain->extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(vkDevice->lDevice, &framebufferInfo, nullptr, &offscreenFramebuffers[frame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create offscreen framebuffer!");
        }
    }
}

void PostProcess::createSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(vkDevice->lDevice, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}

void PostProcess::createFinalBlit() {
    // Final blit uses vkCmdBlitImage, so no pipeline needed
}



void PostProcess::addEffect(const std::string& name, const std::string& computeShaderPath) {
    auto effect = std::make_unique<PostProcessEffect>();
    effect->name = name;
    effect->computeShaderPath = computeShaderPath;
    
    // Create descriptor set layout
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding storageLayoutBinding{};
    storageLayoutBinding.binding = 1;
    storageLayoutBinding.descriptorCount = 1;
    storageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    storageLayoutBinding.pImmutableSamplers = nullptr;
    storageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {samplerLayoutBinding, storageLayoutBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(vkDevice->lDevice, &layoutInfo, nullptr, &effect->descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create effect descriptor set layout!");
    }

    // Create descriptor pool
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT * 2;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT * 2;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT * 2;

    if (vkCreateDescriptorPool(vkDevice->lDevice, &poolInfo, nullptr, &effect->descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create effect descriptor pool!");
    }

    // Allocate descriptor sets
    effect->descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
        std::vector<VkDescriptorSetLayout> layouts(2, effect->descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = effect->descriptorPool;
        allocInfo.descriptorSetCount = 2;
        allocInfo.pSetLayouts = layouts.data();

        if (vkAllocateDescriptorSets(vkDevice->lDevice, &allocInfo, effect->descriptorSets[frame].data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate effect descriptor sets!");
        }
    }

    // Update descriptor sets to point to ping-pong images
    for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
        const auto& images = pingPongImages[frame];
        
        // A->B direction (input=A, output=B)
        {
            VkDescriptorImageInfo imageInfoInput{};
            imageInfoInput.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfoInput.imageView = images.viewA;
            imageInfoInput.sampler = sampler;

            VkDescriptorImageInfo imageInfoOutput{};
            imageInfoOutput.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfoOutput.imageView = images.viewB;
            imageInfoOutput.sampler = VK_NULL_HANDLE;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = effect->descriptorSets[frame][0];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &imageInfoInput;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = effect->descriptorSets[frame][0];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfoOutput;

            vkUpdateDescriptorSets(vkDevice->lDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
        
        // B->A direction (input=B, output=A)
        {
            VkDescriptorImageInfo imageInfoInput{};
            imageInfoInput.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfoInput.imageView = images.viewB;
            imageInfoInput.sampler = sampler;

            VkDescriptorImageInfo imageInfoOutput{};
            imageInfoOutput.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfoOutput.imageView = images.viewA;
            imageInfoOutput.sampler = VK_NULL_HANDLE;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = effect->descriptorSets[frame][1];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &imageInfoInput;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = effect->descriptorSets[frame][1];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfoOutput;

            vkUpdateDescriptorSets(vkDevice->lDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    // Create compute pipeline
    ComputePipelineConfig config{};
    config.setLayouts = {effect->descriptorSetLayout};
    config.compPath = computeShaderPath;

    effect->pipeline = std::make_unique<PipelineCompute>(vkDevice->lDevice, std::move(config));
    effect->pipeline->create();

    effects.push_back(std::move(effect));
}

VkFramebuffer PostProcess::getOffscreenFramebuffer(uint32_t frameIndex) const {
    return offscreenFramebuffers[frameIndex];
}

void PostProcess::executeEffects(VkCommandBuffer cmd, uint32_t frameIndex) {
    if (effects.empty()) return;

    const auto& images = pingPongImages[frameIndex];
    
    // Transition ping-pong image A from COLOR_ATTACHMENT_OPTIMAL to GENERAL for compute
    transitionImageLayout(cmd, images.imageA, VK_FORMAT_R8G8B8A8_UNORM,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    
    // Also transition image B to GENERAL
    transitionImageLayout(cmd, images.imageB, VK_FORMAT_R8G8B8A8_UNORM,
                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    // Pipeline barrier after layout transitions
    VkMemoryBarrier memBarrier{};
    memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        0, 1, &memBarrier, 0, nullptr, 0, nullptr);

    bool inputIsA = true;  // Start with A as input, B as output
    
    // Execute each effect
    for (size_t i = 0; i < effects.size(); ++i) {
        const auto& effect = effects[i];
        
        effect->pipeline->bindCmd(cmd);
        
        // Bind appropriate descriptor set based on ping-pong direction
        int direction = inputIsA ? 0 : 1;  // 0 = A->B, 1 = B->A
        VkDescriptorSet descriptorSet = effect->descriptorSets[frameIndex][direction];
        effect->pipeline->bindSets(cmd, &descriptorSet, 1);
        
        // Dispatch compute work
        uint32_t groupCountX = (swapChain->extent.width + 15) / 16;
        uint32_t groupCountY = (swapChain->extent.height + 15) / 16;
        vkCmdDispatch(cmd, groupCountX, groupCountY, 1);

        // Memory barrier between compute passes
        VkMemoryBarrier computeBarrier{};
        computeBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        computeBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        computeBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(cmd,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 1, &computeBarrier, 0, nullptr, 0, nullptr);

            inputIsA = !inputIsA;  // Swap for next pass
    }
}

void PostProcess::executeFinalBlit(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t swapchainImageIndex) {
    // Get final processed image
    const auto& images = pingPongImages[frameIndex];
    bool finalIsA = (effects.size() % 2) == 0;  // Even number of effects means final result is in A
    VkImage finalImage = finalIsA ? images.imageA : images.imageB;

    // Transition swapchain image to transfer destination
    VkImageMemoryBarrier swapchainBarrier{};
    swapchainBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapchainBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapchainBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchainBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainBarrier.image = swapChain->images[swapchainImageIndex];
    swapchainBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    swapchainBarrier.subresourceRange.baseMipLevel = 0;
    swapchainBarrier.subresourceRange.levelCount = 1;
    swapchainBarrier.subresourceRange.baseArrayLayer = 0;
    swapchainBarrier.subresourceRange.layerCount = 1;
    swapchainBarrier.srcAccessMask = 0;
    swapchainBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &swapchainBarrier);

    // Transition final image to transfer source
    VkImageMemoryBarrier finalImageBarrier{};
    finalImageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    finalImageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    finalImageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    finalImageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    finalImageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    finalImageBarrier.image = finalImage;
    finalImageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    finalImageBarrier.subresourceRange.baseMipLevel = 0;
    finalImageBarrier.subresourceRange.levelCount = 1;
    finalImageBarrier.subresourceRange.baseArrayLayer = 0;
    finalImageBarrier.subresourceRange.layerCount = 1;
    finalImageBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    finalImageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &finalImageBarrier);

    // Blit final image to swapchain
    VkImageBlit blit{};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {static_cast<int32_t>(swapChain->extent.width), static_cast<int32_t>(swapChain->extent.height), 1};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = 0;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {static_cast<int32_t>(swapChain->extent.width), static_cast<int32_t>(swapChain->extent.height), 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = 0;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage(cmd,
                  finalImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  swapChain->images[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  1, &blit, VK_FILTER_LINEAR);

    // Transition swapchain image to present
    VkImageMemoryBarrier presentBarrier{};
    presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    presentBarrier.image = swapChain->images[swapchainImageIndex];
    presentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    presentBarrier.subresourceRange.baseMipLevel = 0;
    presentBarrier.subresourceRange.levelCount = 1;
    presentBarrier.subresourceRange.baseArrayLayer = 0;
    presentBarrier.subresourceRange.layerCount = 1;
    presentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    presentBarrier.dstAccessMask = 0;

    vkCmdPipelineBarrier(cmd,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &presentBarrier);
}

VkImageView PostProcess::getFinalImageView(uint32_t frameIndex) const {
    // Return the final image based on whether we have even or odd number of effects
    const auto& images = pingPongImages[frameIndex];
    bool finalIsA = (effects.size() % 2) == 0;  // Even number of effects means final result is in A
    return finalIsA ? images.viewA : images.viewB;
}

void PostProcess::recreate() {
    cleanup();
    initialize();
    
    // Recreate all effect descriptor sets with new images
    for (auto& effect : effects) {
        // Update descriptor sets to point to new ping-pong images
        for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
            const auto& images = pingPongImages[frame];
            
            // A->B direction
            {
                VkDescriptorImageInfo imageInfoInput{};
                imageInfoInput.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                imageInfoInput.imageView = images.viewA;
                imageInfoInput.sampler = sampler;

                VkDescriptorImageInfo imageInfoOutput{};
                imageInfoOutput.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                imageInfoOutput.imageView = images.viewB;
                imageInfoOutput.sampler = VK_NULL_HANDLE;

                std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
                descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[0].dstSet = effect->descriptorSets[frame][0];
                descriptorWrites[0].dstBinding = 0;
                descriptorWrites[0].dstArrayElement = 0;
                descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorWrites[0].descriptorCount = 1;
                descriptorWrites[0].pImageInfo = &imageInfoInput;

                descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[1].dstSet = effect->descriptorSets[frame][0];
                descriptorWrites[1].dstBinding = 1;
                descriptorWrites[1].dstArrayElement = 0;
                descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                descriptorWrites[1].descriptorCount = 1;
                descriptorWrites[1].pImageInfo = &imageInfoOutput;

                vkUpdateDescriptorSets(vkDevice->lDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
            }
            
            // B->A direction
            {
                VkDescriptorImageInfo imageInfoInput{};
                imageInfoInput.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                imageInfoInput.imageView = images.viewB;
                imageInfoInput.sampler = sampler;

                VkDescriptorImageInfo imageInfoOutput{};
                imageInfoOutput.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                imageInfoOutput.imageView = images.viewA;
                imageInfoOutput.sampler = VK_NULL_HANDLE;

                std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
                descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[0].dstSet = effect->descriptorSets[frame][1];
                descriptorWrites[0].dstBinding = 0;
                descriptorWrites[0].dstArrayElement = 0;
                descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorWrites[0].descriptorCount = 1;
                descriptorWrites[0].pImageInfo = &imageInfoInput;

                descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[1].dstSet = effect->descriptorSets[frame][1];
                descriptorWrites[1].dstBinding = 1;
                descriptorWrites[1].dstArrayElement = 0;
                descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                descriptorWrites[1].descriptorCount = 1;
                descriptorWrites[1].pImageInfo = &imageInfoOutput;

                vkUpdateDescriptorSets(vkDevice->lDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
            }
        }
    }
}

void PostProcess::createImage(uint32_t width, uint32_t height, VkFormat format,
                             VkImageTiling tiling, VkImageUsageFlags usage,
                             VkMemoryPropertyFlags properties, VkImage& image,
                             VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(vkDevice->lDevice, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(vkDevice->lDevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(vkDevice->lDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(vkDevice->lDevice, image, imageMemory, 0);
}

VkImageView PostProcess::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(vkDevice->lDevice, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
}

uint32_t PostProcess::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(vkDevice->pDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}



void PostProcess::transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkFormat format,
                                      VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    // Determine if this is a depth or color image
    bool isDepth = (format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT);
    barrier.subresourceRange.aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void PostProcess::cleanup() {
    VkDevice device = vkDevice->lDevice;
    
    // Clean up effects
    for (auto& effect : effects) {
        effect->cleanup(device);
    }
    effects.clear();
    
    // Final blit cleanup (nothing to clean up for vkCmdBlitImage approach)
    

    
    // Clean up offscreen resources
    for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
        if (offscreenFramebuffers[frame] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, offscreenFramebuffers[frame], nullptr);
            offscreenFramebuffers[frame] = VK_NULL_HANDLE;
        }
    }
    
    if (offscreenRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, offscreenRenderPass, nullptr);
        offscreenRenderPass = VK_NULL_HANDLE;
    }
    
    // Clean up ping-pong images
    for (auto& images : pingPongImages) {
        images.cleanup(device);
    }
    
    // Clean up sampler
    if (sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, sampler, nullptr);
        sampler = VK_NULL_HANDLE;
    }
}
