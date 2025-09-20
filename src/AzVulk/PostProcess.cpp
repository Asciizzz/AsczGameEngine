#include "AzVulk/PostProcess.hpp"
#include "AzVulk/TextureVK.hpp"
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

void destroyImageViews(VkDevice lDevice, VkImageView view) {
    if (view != VK_NULL_HANDLE) {
        vkDestroyImageView(lDevice, view, nullptr);
        view = VK_NULL_HANDLE;
    }
}

void destroyDeviceMemory(VkDevice lDevice, VkDeviceMemory memory) {
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(lDevice, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

void PostProcessEffect::cleanup(VkDevice device) {
    if (pipeline) {
        pipeline->cleanup();
        pipeline.reset();
    }
}

PostProcess::PostProcess(Device* deviceVK, SwapChain* swapChain, DepthManager* depthManager)
    : deviceVK(deviceVK), swapChain(swapChain), depthManager(depthManager) {
}

PostProcess::~PostProcess() {
    cleanup();
}

void PostProcess::initialize(VkRenderPass offscreenRenderPass) {
    this->offscreenRenderPass = offscreenRenderPass;
    
    // CRITICAL: Ensure device is idle before creating resources
    vkDeviceWaitIdle(deviceVK->lDevice);
    
    // Validate dependencies before proceeding
    if (!swapChain || !depthManager || !deviceVK) {
        throw std::runtime_error("PostProcess: Invalid dependencies during initialization");
    }
    
    // Validate swapchain extent is valid
    if (swapChain->extent.width == 0 || swapChain->extent.height == 0) {
        throw std::runtime_error("PostProcess: Invalid swapchain dimensions");
    }

    createSampler();
    createPingPongImages();
    createOffscreenFramebuffers();
    createSharedDescriptors();
    createFinalBlit();
}

void PostProcess::createPingPongImages() {
    // Use R8G8B8A8_UNORM format which supports storage images
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    VkExtent2D extent = swapChain->extent;
    
    for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
        pingPongImages.push_back(MakeUnique<PingPongImages>());
        auto& imageA = pingPongImages[frame]->imageA;
        auto& imageB = pingPongImages[frame]->imageB;

        ImageConfig sharedImageConfig = ImageConfig()
            .setDimensions(extent.width, extent.height)
            .setFormat(format)
            .setTiling(VK_IMAGE_TILING_OPTIMAL)
            .setUsage(  ImageUsage::ColorAttach |
                        ImageUsage::Sampled | 
                        ImageUsage::Storage |
                        ImageUsage::TransferSrc |
                        ImageUsage::TransferDst)
            .setMemProps(MemProp::DeviceLocal);

        ImageViewConfig sharedViewConfig = ImageViewConfig()
            .setAspectMask(VK_IMAGE_ASPECT_COLOR_BIT);

        bool success = 
            imageA
                .init(deviceVK)
                .createImage(sharedImageConfig)
                .createView(sharedViewConfig)
                .isValid() &&
            imageB
                .init(deviceVK)
                .createImage(sharedImageConfig)
                .createView(sharedViewConfig)
                .isValid();

        if (!success) throw std::runtime_error("Failed to create ping-pong images");
    }
}

void PostProcess::createOffscreenFramebuffers() {
    for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
        std::array<VkImageView, 2> attachments = {
            pingPongImages[frame]->getViewA(),        // Color attachment (index 0)
            depthManager->getDepthImageView()   // Depth attachment (index 1)
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = offscreenRenderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChain->extent.width;
        framebufferInfo.height = swapChain->extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(deviceVK->lDevice, &framebufferInfo, nullptr, &offscreenFramebuffers[frame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create offscreen framebuffer!");
        }
    }
}

void PostProcess::createSampler() {
    // VkSamplerCreateInfo samplerInfo{};
    // samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    // samplerInfo.magFilter = VK_FILTER_LINEAR;
    // samplerInfo.minFilter = VK_FILTER_LINEAR;
    // samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    // samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    // samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    // samplerInfo.anisotropyEnable = VK_FALSE;
    // samplerInfo.maxAnisotropy = 1.0f;
    // samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    // samplerInfo.unnormalizedCoordinates = VK_FALSE;
    // samplerInfo.compareEnable = VK_FALSE;
    // samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    // samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    // samplerInfo.mipLodBias = 0.0f;
    // samplerInfo.minLod = 0.0f;
    // samplerInfo.maxLod = 0.0f;

    SamplerConfig config = SamplerConfig()
        .setFilters(VK_FILTER_LINEAR, VK_FILTER_LINEAR)
        .setAddressModes(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
        .setAnisotropy(VK_FALSE)
        .setBorderColor(VK_BORDER_COLOR_INT_OPAQUE_BLACK)
        .setCompare(VK_FALSE)
        .setMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
        .setLodRange(0.0f, 0.0f);

    sampler = MakeUnique<SamplerVK>();
    sampler->init(deviceVK).create(config);
}

void PostProcess::createSharedDescriptors() {
    descriptorSets = MakeUnique<DescSet>(deviceVK->lDevice);

    // Create descriptor set layout with validation
    descriptorSets->createOwnLayout({
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    });

    // Create descriptor pool with validation
    descriptorSets->createOwnPool({
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * 4}, // Color input + depth for each direction
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT * 2}
    }, MAX_FRAMES_IN_FLIGHT * 2);

    // Allocate descriptor sets with validation
    descriptorSets->allocate(MAX_FRAMES_IN_FLIGHT * 2);

    // Update descriptor sets to point to ping-pong images
    for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
        const auto& images = pingPongImages[frame];
        
        // A->B direction (descriptor set index: frame * 2 + 0)
        {
            VkDescriptorImageInfo imageInfoInput{};
            imageInfoInput.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfoInput.imageView = images->getViewA();
            imageInfoInput.sampler = *sampler;

            VkDescriptorImageInfo imageInfoOutput{};
            imageInfoOutput.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfoOutput.imageView = images->getViewB();
            imageInfoOutput.sampler = VK_NULL_HANDLE;

            VkDescriptorImageInfo imageInfoDepth{};
            imageInfoDepth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            imageInfoDepth.imageView = depthManager->getDepthImageView();
            imageInfoDepth.sampler = *sampler;

            std::array<VkWriteDescriptorSet, 3> descriptorWrites{};
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = descriptorSets->get(frame * 2 + 0);
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &imageInfoInput;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = descriptorSets->get(frame * 2 + 0);
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfoOutput;

            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = descriptorSets->get(frame * 2 + 0);
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pImageInfo = &imageInfoDepth;

            vkUpdateDescriptorSets(deviceVK->lDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
        
        // B->A direction (descriptor set index: frame * 2 + 1)
        {
            VkDescriptorImageInfo imageInfoInput{};
            imageInfoInput.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfoInput.imageView = images->getViewB();
            imageInfoInput.sampler = *sampler;

            VkDescriptorImageInfo imageInfoOutput{};
            imageInfoOutput.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfoOutput.imageView = images->getViewA();
            imageInfoOutput.sampler = VK_NULL_HANDLE;

            VkDescriptorImageInfo imageInfoDepth{};
            imageInfoDepth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            imageInfoDepth.imageView = depthManager->getDepthImageView();
            imageInfoDepth.sampler = *sampler;

            std::array<VkWriteDescriptorSet, 3> descriptorWrites{};
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = descriptorSets->get(frame * 2 + 1);
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &imageInfoInput;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = descriptorSets->get(frame * 2 + 1);
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfoOutput;

            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = descriptorSets->get(frame * 2 + 1);
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pImageInfo = &imageInfoDepth;

            vkUpdateDescriptorSets(deviceVK->lDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }
}

void PostProcess::createFinalBlit() {
    // Final blit uses vkCmdBlitImage, so no pipeline needed
}



void PostProcess::addEffect(const std::string& name, const std::string& computeShaderPath) {
    addEffect(name, computeShaderPath, true); // Default to active
}

void PostProcess::addEffect(const std::string& name, const std::string& computeShaderPath, bool active) {
    // Validate shader file exists before attempting to create pipeline
    std::ifstream testFile(computeShaderPath, std::ios::binary);
    if (!testFile.is_open()) {
        throw std::runtime_error("PostProcess: Cannot open shader file: " + computeShaderPath);
    }
    testFile.close();
    
    auto effect = MakeUnique<PostProcessEffect>();
    effect->computeShaderPath = computeShaderPath;
    effect->active = active;

    try {
        // Create compute pipeline using the shared descriptor set layout
        ComputePipelineConfig config{};
        config.setLayouts = {descriptorSets->getLayout()};
        config.compPath = computeShaderPath;

        effect->pipeline = MakeUnique<PipelineCompute>(deviceVK->lDevice, std::move(config));
        effect->pipeline->create();

        // Store in OrderedMap with name as key
        effects[name] = std::move(effect);
    } catch (const std::exception& e) {
        throw std::runtime_error("PostProcess: Failed to create effect '" + name + "': " + e.what());
    }
}

void PostProcess::loadEffectsFromJson(const std::string& configPath) {
    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open postprocess config file: " << configPath << std::endl;
            return;
        }

        nlohmann::json config;
        file >> config;

        if (config.contains("effects") && config["effects"].is_array()) {
            for (const auto& effectConfig : config["effects"]) {
                if (effectConfig.contains("name") && effectConfig.contains("shader")) {
                    std::string name = effectConfig["name"];
                    std::string shader = effectConfig["shader"];
                    bool active = effectConfig.value("active", true); // Default to true if not specified

                    try {
                        addEffect(name, shader, active);
                    } catch (const std::exception& e) {
                        std::cerr << "Error loading effect '" << name << "': " << e.what() << std::endl;
                        // Continue loading other effects instead of crashing
                        continue;
                    }
                } else {
                    std::cerr << "Warning: Effect configuration missing 'name' or 'shader' field" << std::endl;
                }
            }
        } else {
            std::cerr << "Warning: Invalid postprocess config format - missing 'effects' array" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading postprocess config: " << e.what() << std::endl;
        // Don't rethrow - allow application to continue with no effects
    }
}

VkFramebuffer PostProcess::getOffscreenFramebuffer(uint32_t frameIndex) const {
    return offscreenFramebuffers[frameIndex];
}

void PostProcess::executeEffects(VkCommandBuffer cmd, uint32_t frameIndex) {
    if (effects.empty()) return;

    const auto& images = pingPongImages[frameIndex];
    
    // Images are already in GENERAL layout from the render pass, so no transition needed for image A
    // Just transition image B from UNDEFINED to GENERAL
    transitionImageLayout(cmd, images->getImageB(), VK_FORMAT_R8G8B8A8_UNORM,
                        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    // Transition depth image to read-only layout for compute shader access
    VkImageMemoryBarrier depthBarrier{};
    depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    depthBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthBarrier.image = depthManager->getDepthImage();
    depthBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthBarrier.subresourceRange.baseMipLevel = 0;
    depthBarrier.subresourceRange.levelCount = 1;
    depthBarrier.subresourceRange.baseArrayLayer = 0;
    depthBarrier.subresourceRange.layerCount = 1;
    depthBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depthBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &depthBarrier);

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
    
    // Execute each active effect in order
    for (const auto& [name, effect] : effects) {
        if (!effect->active) continue;  // Skip inactive effects
        
        effect->pipeline->bindCmd(cmd);
        
        // Bind appropriate shared descriptor set based on ping-pong direction
        int direction = inputIsA ? 0 : 1;  // 0 = A->B, 1 = B->A
        VkDescriptorSet descriptorSet = descriptorSets->get(frameIndex * 2 + direction);
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

    // Transition depth image back to attachment optimal layout
    VkImageMemoryBarrier depthBackBarrier{};
    depthBackBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    depthBackBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depthBackBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthBackBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthBackBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthBackBarrier.image = depthManager->getDepthImage();
    depthBackBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthBackBarrier.subresourceRange.baseMipLevel = 0;
    depthBackBarrier.subresourceRange.levelCount = 1;
    depthBackBarrier.subresourceRange.baseArrayLayer = 0;
    depthBackBarrier.subresourceRange.layerCount = 1;
    depthBackBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    depthBackBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &depthBackBarrier);
}

void PostProcess::executeFinalBlit(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t swapchainImageIndex) {
    // Count active effects to determine final image location
    size_t activeEffectCount = 0;
    for (const auto& [name, effect] : effects) {
        if (effect->active) activeEffectCount++;
    }
    
    // Get final processed image
    const auto& images = pingPongImages[frameIndex];
    bool finalIsA = (activeEffectCount % 2) == 0;  // Even number of effects means final result is in A
    VkImage finalImage = finalIsA ? images->getImageA() : images->getImageB();

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

    vkCmdBlitImage(cmd, finalImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
    // Count active effects to determine final image location
    size_t activeEffectCount = 0;
    for (const auto& [name, effect] : effects) {
        if (effect->active) activeEffectCount++;
    }
    
    // Return the final image based on whether we have even or odd number of active effects
    const auto& images = pingPongImages[frameIndex];
    bool finalIsA = (activeEffectCount % 2) == 0;  // Even number of effects means final result is in A
    return finalIsA ? images->getViewA() : images->getViewB();
}

void PostProcess::recreate() {
    // Clean up render resources but preserve effect configurations
    cleanupRenderResources();
    
    // Recreate render resources
    createSampler();
    createPingPongImages();
    createOffscreenFramebuffers();
    createSharedDescriptors();
    
    // Recreate effects from stored configurations
    recreateEffects();
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

void PostProcess::cleanupRenderResources() {
    VkDevice device = deviceVK->lDevice;
    
    // Wait for device to be idle to ensure no resources are in use
    vkDeviceWaitIdle(device);
    
    // Clean up framebuffers first (before image views)
    for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
        if (offscreenFramebuffers[frame] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, offscreenFramebuffers[frame], nullptr);
            offscreenFramebuffers[frame] = VK_NULL_HANDLE;
        }
    }
    
    // Clean up descriptor sets
    descriptorSets->cleanup();

    // Clean up sampler
    sampler->cleanup();

    // Destroy ping-pong images
    pingPongImages.clear();
}

void PostProcess::recreateEffects() {
    // Store the effect configurations before cleanup
    OrderedMap<std::string, std::string> storedConfigs;
    OrderedMap<std::string, bool> storedActiveStates;
    for (const auto& [name, effect] : effects) {
        storedConfigs[name] = effect->computeShaderPath;
        storedActiveStates[name] = effect->active;
    }
    
    // Clean up current effects first
    VkDevice device = deviceVK->lDevice;
    for (auto& [name, effect] : effects) {
        effect->cleanup(device);
    }
    effects.clear();
    
    // Recreate all stored effects
    for (const auto& [name, shaderPath] : storedConfigs) {
        auto effect = MakeUnique<PostProcessEffect>();
        effect->computeShaderPath = shaderPath;
        effect->active = storedActiveStates[name];  // Restore active state

        // Create compute pipeline using the shared descriptor set layout
        ComputePipelineConfig config{};
        config.setLayouts = {descriptorSets->getLayout()};
        config.compPath = shaderPath;

        effect->pipeline = MakeUnique<PipelineCompute>(deviceVK->lDevice, std::move(config));
        effect->pipeline->create();

        effects[name] = std::move(effect);
    }
}

void PostProcess::cleanup() {
    VkDevice device = deviceVK->lDevice;

    // Wait for device to be idle to ensure no resources are in use
    vkDeviceWaitIdle(device);

    // Clean up all effects
    for (auto& [name, effect] : effects) {
        effect->cleanup(device);
    }
    effects.clear();

    // Clean up all render resources
    cleanupRenderResources();
}
