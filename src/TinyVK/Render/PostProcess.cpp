#include "TinyVK/Render/PostProcess.hpp"
#include "TinyVK/Resource/TextureVK.hpp"
#include <stdexcept>
#include <iostream>

#include ".ext/json/json.hpp"

using namespace TinyVK;

// Destroy helper

void PostProcessEffect::cleanup(VkDevice device) {
    if (pipeline) {
        pipeline->cleanup();
        pipeline.reset();
    }
}

PostProcess::PostProcess(Device* deviceVK, Swapchain* swapchain, DepthImage* depthImage)
    : deviceVK(deviceVK), swapchain(swapchain), depthImage(depthImage) {
}

PostProcess::~PostProcess() {
    cleanup();
}

void PostProcess::initialize() {
    // CRITICAL: Ensure device is idle before creating resources
    vkDeviceWaitIdle(deviceVK->device);
    
    // Validate dependencies before proceeding
    if (!swapchain || !depthImage || !deviceVK) {
        throw std::runtime_error("PostProcess: Invalid dependencies during initialization");
    }
    
    // Validate swapchain extent is valid
    if (swapchain->compareExtent({0, 0})) {
        throw std::runtime_error("PostProcess: Invalid swapchain dimensions");
    }

    createOffscreenRenderPass();
    createSampler();
    createPingPongImages();
    createOffscreenFrameBuffers();
    createOffscreenRenderTargets();
    createSharedDescriptors();
    createFinalBlit();
}

void PostProcess::createOffscreenRenderPass() {
    // Create offscreen render pass for scene rendering to ping-pong images
    VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depthFormat = depthImage->getFormat();
    
    RenderPassConfig config = RenderPassConfig::offscreenRendering(colorFormat, depthFormat);
    
    offscreenRenderPass = MakeUnique<RenderPass>(deviceVK->device, config);
}

void PostProcess::createPingPongImages() {
    // Use R8G8B8A8_UNORM format which supports storage images
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    VkExtent2D extent = swapchain->getExtent();

    VkDevice device = deviceVK->device;
    VkPhysicalDevice pDevice = deviceVK->pDevice;
    
    for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
        pingPongImages.push_back(MakeUnique<PingPongImages>());
        auto& imageA = pingPongImages[frame]->imageA;
        auto& imageB = pingPongImages[frame]->imageB;

        ImageConfig sharedImageConfig = ImageConfig()
            .withPhysicalDevice(pDevice)
            .withDimensions(extent.width, extent.height)
            .withFormat(format)
            .withTiling(VK_IMAGE_TILING_OPTIMAL)
            .withUsage(  ImageUsage::ColorAttach |
                        ImageUsage::Sampled | 
                        ImageUsage::Storage |
                        ImageUsage::TransferSrc |
                        ImageUsage::TransferDst)
            .withMemProps(MemProp::DeviceLocal);

        ImageViewConfig sharedViewConfig = ImageViewConfig()
            .withAspectMask(VK_IMAGE_ASPECT_COLOR_BIT);

        bool success = 
            imageA
                .init(device)
                .createImage(sharedImageConfig)
                .createView(sharedViewConfig)
                .isValid() &&
            imageB
                .init(device)
                .createImage(sharedImageConfig)
                .createView(sharedViewConfig)
                .isValid();

        if (!success) throw std::runtime_error("Failed to create ping-pong images");
    }
}

void PostProcess::createOffscreenFrameBuffers() {
    offscreenFrameBuffers.clear();
    
    for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
        UniquePtr<FrameBuffer> framebuffer = MakeUnique<FrameBuffer>(deviceVK->device);

        FrameBufferConfig fbConfig = FrameBufferConfig()
            .withRenderPass(offscreenRenderPass->get())
            .addAttachment(pingPongImages[frame]->getViewA())  // Color attachment (index 0)
            .addAttachment(depthImage->getView())  // Depth attachment (index 1)
            .withExtent(swapchain->getExtent());

        bool success = framebuffer->create(fbConfig);
        if (!success) throw std::runtime_error("Failed to create offscreen framebuffer");

        offscreenFrameBuffers.push_back(std::move(framebuffer));
    }
}

void PostProcess::createOffscreenRenderTargets() {
    offscreenRenderTargets.clear();
    offscreenRenderTargets.reserve(MAX_FRAMES_IN_FLIGHT);
    
    for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
        const auto& images = pingPongImages[frame];
        
        // Create render target with attachments
        RenderTarget renderTarget(*offscreenRenderPass, *offscreenFrameBuffers[frame], swapchain->getExtent());

        // Add color attachment (ping-pong image A)
        VkClearValue colorClear{};
        colorClear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        renderTarget.addAttachment(images->getImageA(), images->getViewA(), colorClear);
        
        // Add depth attachment
        VkClearValue depthClear{};
        depthClear.depthStencil = {1.0f, 0};
        renderTarget.addAttachment(depthImage->getImage(), depthImage->getView(), depthClear);
        
        offscreenRenderTargets.push_back(std::move(renderTarget));
    }
}

void PostProcess::createSampler() {
    SamplerConfig config = SamplerConfig()
        .withFilters(VK_FILTER_LINEAR, VK_FILTER_LINEAR)
        .withAddressModes(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
        .withAnisotropy(VK_FALSE) // No anisotropy for post-process
        .withCompare(VK_FALSE) // No compare operation
        .withBorderColor(VK_BORDER_COLOR_INT_OPAQUE_BLACK)
        .withLodRange(0.0f, 0.0f);

    sampler = MakeUnique<SamplerVK>();
    sampler->init(*deviceVK).create(config);
}

void PostProcess::createSharedDescriptors() {
    VkDevice device = deviceVK->device;

    // Create descriptor set layout with validation
    descLayout = MakeUnique<DescLayout>();
    descLayout->create(device, {
        {0, DescType::CombinedImageSampler, 1, ShaderStage::Compute, nullptr},
        {1, DescType::StorageImage,         1, ShaderStage::Compute, nullptr},
        {2, DescType::CombinedImageSampler, 1, ShaderStage::Compute, nullptr}
    });

    // Create descriptor pool with validation
    descPool = MakeUnique<DescPool>();
    descPool->create(device, {
        {DescType::CombinedImageSampler, MAX_FRAMES_IN_FLIGHT * 4}, // Color input + depth for each direction
        {DescType::StorageImage, MAX_FRAMES_IN_FLIGHT * 2}
    }, MAX_FRAMES_IN_FLIGHT * 2);

    // Allocate descriptor sets with validation
    descSets.clear();  // Clear old invalid handles

    // Update descriptor sets to point to ping-pong images
    for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
        UniquePtr<DescSet> descSet0 = MakeUnique<DescSet>();
        descSet0->allocate(device, *descPool, *descLayout);

        UniquePtr<DescSet> descSet1 = MakeUnique<DescSet>();
        descSet1->allocate(device, *descPool, *descLayout);

        const auto& images = pingPongImages[frame];

        VkDescriptorImageInfo imageInfoInputA{};
        imageInfoInputA.imageLayout = ImageLayout::General;
        imageInfoInputA.imageView = images->getViewA();
        imageInfoInputA.sampler = *sampler;

        VkDescriptorImageInfo imageInfoOutputB{};
        imageInfoOutputB.imageLayout = ImageLayout::General;
        imageInfoOutputB.imageView = images->getViewB();
        imageInfoOutputB.sampler = VK_NULL_HANDLE;

        VkDescriptorImageInfo imageInfoInputB{};
        imageInfoInputB.imageLayout = ImageLayout::General;
        imageInfoInputB.imageView = images->getViewB();
        imageInfoInputB.sampler = *sampler;

        VkDescriptorImageInfo imageInfoOutputA{};
        imageInfoOutputA.imageLayout = ImageLayout::General;
        imageInfoOutputA.imageView = images->getViewA();
        imageInfoOutputA.sampler = VK_NULL_HANDLE;

        VkDescriptorImageInfo imageInfoDepth{};
        imageInfoDepth.imageLayout = ImageLayout::DepthStencilReadOnlyOptimal;
        imageInfoDepth.imageView = depthImage->getView();
        imageInfoDepth.sampler = *sampler;

        DescWrite()
            // A -> B
            .addWrite().setDstBinding(0).setDstSet(*descSet0).setDescType(DescType::CombinedImageSampler).setImageInfo({imageInfoInputA})
            .addWrite().setDstBinding(1).setDstSet(*descSet0).setDescType(DescType::StorageImage).setImageInfo({imageInfoOutputB})
            .addWrite().setDstBinding(2).setDstSet(*descSet0).setDescType(DescType::CombinedImageSampler).setImageInfo({imageInfoDepth})
            // B -> A
            .addWrite().setDstBinding(0).setDstSet(*descSet1).setDescType(DescType::CombinedImageSampler).setImageInfo({imageInfoInputB})
            .addWrite().setDstBinding(1).setDstSet(*descSet1).setDescType(DescType::StorageImage).setImageInfo({imageInfoOutputA})
            .addWrite().setDstBinding(2).setDstSet(*descSet1).setDescType(DescType::CombinedImageSampler).setImageInfo({imageInfoDepth})
            .updateDescSets(device);

        descSets.push_back(std::move(descSet0));
        descSets.push_back(std::move(descSet1));
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
        config.setLayouts = {*descLayout};
        config.compPath = computeShaderPath;

        effect->pipeline = MakeUnique<PipelineCompute>(deviceVK->device, std::move(config));
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

RenderTarget* PostProcess::getOffscreenRenderTarget(uint32_t frameIndex) {
    if (frameIndex >= offscreenRenderTargets.size()) return nullptr;
    return &offscreenRenderTargets[frameIndex];
}

VkFramebuffer PostProcess::getOffscreenFrameBuffer(uint32_t frameIndex) const {
    return *offscreenFrameBuffers[frameIndex];
}

void PostProcess::executeEffects(VkCommandBuffer cmd, uint32_t frameIndex) {
    if (effects.empty()) return;

    const auto& images = pingPongImages[frameIndex];
    
    // Images are already in GENERAL layout from the render pass, so no transition needed for image A
    // Just transition image B from UNDEFINED to GENERAL
    transitionImageLayout(cmd, images->getImageB(), VK_FORMAT_R8G8B8A8_UNORM,
                        ImageLayout::Undefined, ImageLayout::General);

    // Transition depth image to read-only layout for compute shader access
    VkImageMemoryBarrier depthBarrier{};
    depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    depthBarrier.oldLayout = ImageLayout::DepthStencilAttachmentOptimal;
    depthBarrier.newLayout = ImageLayout::DepthStencilReadOnlyOptimal;
    depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthBarrier.image = depthImage->getImage();
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
        VkDescriptorSet descriptorSet = *descSets[frameIndex * 2 + direction];
        effect->pipeline->bindSets(cmd, &descriptorSet, 1);
        
        // Dispatch compute work
        uint32_t groupCountX = (swapchain->getWidth() + 15) / 16;
        uint32_t groupCountY = (swapchain->getHeight() + 15) / 16;
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
    depthBackBarrier.oldLayout = ImageLayout::DepthStencilReadOnlyOptimal;
    depthBackBarrier.newLayout = ImageLayout::DepthStencilAttachmentOptimal;
    depthBackBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthBackBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthBackBarrier.image = depthImage->getImage();
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
    swapchainBarrier.image = swapchain->getImage(swapchainImageIndex);
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
    finalImageBarrier.oldLayout = ImageLayout::General;
    finalImageBarrier.newLayout = ImageLayout::TransferSrcOptimal;
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
    blit.srcOffsets[1] = {static_cast<int32_t>(swapchain->getWidth()), static_cast<int32_t>(swapchain->getHeight()), 1};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = 0;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {static_cast<int32_t>(swapchain->getWidth()), static_cast<int32_t>(swapchain->getHeight()), 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = 0;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage(cmd, finalImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  swapchain->getImage(swapchainImageIndex), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  1, &blit, VK_FILTER_LINEAR);

    // Transition swapchain image to present
    VkImageMemoryBarrier presentBarrier{};
    presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    presentBarrier.image = swapchain->getImage(swapchainImageIndex);
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
    createOffscreenRenderPass();
    createSampler();
    createPingPongImages();
    createOffscreenFrameBuffers();
    createOffscreenRenderTargets();
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

    if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == ImageLayout::General) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    } else if (oldLayout == ImageLayout::Undefined && newLayout == ImageLayout::General) {
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
    VkDevice device = deviceVK->device;

    vkDeviceWaitIdle(device);

    offscreenRenderTargets.clear();
    
    offscreenFrameBuffers.clear();

    descSets.clear();

    sampler->cleanup();

    pingPongImages.clear();

    offscreenRenderPass.reset();
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
    VkDevice device = deviceVK->device;
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
        config.setLayouts = {*descLayout};
        config.compPath = shaderPath;

        effect->pipeline = MakeUnique<PipelineCompute>(deviceVK->device, std::move(config));
        effect->pipeline->create();

        effects[name] = std::move(effect);
    }
}

void PostProcess::cleanup() {
    VkDevice device = deviceVK->device;

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
