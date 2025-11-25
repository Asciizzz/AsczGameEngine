#include "tinyVk/Render/Renderer.hpp"

#include <stdexcept>
#include <cstring>
#include <SDL2/SDL.h>

using namespace tinyVk;


Renderer::Renderer (Device* dvk, VkSurfaceKHR surface, SDL_Window* window, uint32_t maxFramesInFlight)
: dvk(dvk), maxFramesInFlight(maxFramesInFlight) {

    swapchain = MakeUnique<Swapchain>(dvk, surface, window);

    depthImage = MakeUnique<DepthImage>(dvk->device);
    depthImage->create(dvk->pDevice, swapchain->getExtent());

    createRenderTargets();

    createCommandBuffers();
    createSyncObjects();
}

Renderer::~Renderer() {
    VkDevice device = dvk->device;

    for (size_t i = 0; i < maxFramesInFlight; ++i) {
        if (inFlightFences[i])           vkDestroyFence(    device, inFlightFences[i],           nullptr);
        if (imageAvailableSemaphores[i]) vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
    }
    for (size_t i = 0; i < renderFinishedSemaphores.size(); ++i) {
        if (renderFinishedSemaphores[i]) vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
    }
    
    // RenderTargets are automatically cleaned up (non-owning)
    swapchainRenderTargets.clear();
}

void Renderer::createCommandBuffers() {
    cmdBuffers.create(dvk->device, dvk->graphicsPoolWrapper.pool, maxFramesInFlight);
}

void Renderer::createSyncObjects() {
    swapchainImageCount = swapchain->images.size();

    imageAvailableSemaphores.resize(maxFramesInFlight);
    inFlightFences.resize(maxFramesInFlight);

    // IMPORTANT: one render-finished semaphore *per swapchain image*
    renderFinishedSemaphores.resize(swapchainImageCount);

    // track which image is tied to which fence
    imagesInFlight.resize(swapchainImageCount, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    // per-frame acquire + fence
    for (size_t i = 0; i < maxFramesInFlight; ++i) {
        if (vkCreateSemaphore(dvk->device, &semInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(    dvk->device, &fenceInfo, nullptr, &inFlightFences[i])          != VK_SUCCESS) {
            throw std::runtime_error("failed to create per-frame sync objects!");
        }
    }

    // per-image render-finished
    for (size_t i = 0; i < swapchainImageCount; ++i) {
        if (vkCreateSemaphore(dvk->device, &semInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create per-image renderFinished semaphore!");
        }
    }
}

void Renderer::createRenderTargets() {
    VkDevice device = dvk->device;
    VkExtent2D extent = swapchain->getExtent();
    
    // Clear existing targets and resources
    swapchainRenderTargets.clear();
    framebuffers.clear();
    
    // Create render passes with proper ownership
    auto mainRenderPassConfig = RenderPassConfig::forwardRendering(
        swapchain->getImageFormat(), 
        depthImage->getFormat()
    );
    mainRenderPass = MakeUnique<RenderPass>(device, mainRenderPassConfig);
    
    // Create swapchain framebuffers and render targets
    framebuffers.reserve(swapchain->getImageCount());
    for (uint32_t i = 0; i < swapchain->getImageCount(); ++i) {
        // Create framebuffer for this swapchain image
        FrameBufferConfig fbConfig = FrameBufferConfig()
            .withRenderPass(mainRenderPass->get())
            .addAttachment(swapchain->getImageView(i))
            .addAttachment(depthImage->getView())
            .withExtent(extent);

        auto framebuffer = MakeUnique<FrameBuffer>(device);
        bool success = framebuffer->create(fbConfig);
        if (!success) throw std::runtime_error("Failed to create swapchain framebuffer");

        // Create render target
        RenderTarget swapchainTarget = RenderTarget()
            .withRenderPass(*mainRenderPass)
            .withFrameBuffer(framebuffer->get())
            .withExtent(extent);

        // Add color attachment info
        VkClearValue colorClear;
        colorClear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkClearValue depthClear;
        depthClear.depthStencil = {1.0f, 0};
        
        swapchainTarget.addAttachment(swapchain->getImage(i), swapchain->getImageView(i), colorClear);
        swapchainTarget.addAttachment(depthImage->getImage(), depthImage->getView(), depthClear);
        
        swapchainRenderTargets.push_back(swapchainTarget);
        
        // Store framebuffer with proper ownership
        framebuffers.push_back(std::move(framebuffer));
    }
}

VkRenderPass Renderer::getMainRenderPass() const {
    return mainRenderPass ? mainRenderPass->get() : VK_NULL_HANDLE;
}

VkFramebuffer Renderer::getFrameBuffer(uint32_t imageIndex) const {
    return (imageIndex < framebuffers.size() && framebuffers[imageIndex]) ? 
           framebuffers[imageIndex]->get() : VK_NULL_HANDLE;
}

VkExtent2D Renderer::getSwapChainExtent() const {
    return swapchain ? swapchain->getExtent() : VkExtent2D{0, 0};
}

VkCommandBuffer Renderer::getCurrentCommandBuffer() const {
    return cmdBuffers[currentFrame];
}

void Renderer::handleWindowResize(SDL_Window* window) {
    // Wait for device to be idle
    vkDeviceWaitIdle(dvk->device);
    
    // Get new window dimensions for depth resources
    int newWidth, newHeight;
    SDL_GetWindowSize(window, &newWidth, &newHeight);
    
    // Recreate depth resources before recreating other resources
    depthImage->create(dvk->pDevice, newWidth, newHeight);
    
    // Now safe to cleanup and recreate Swapchain
    swapchain->cleanup();
    swapchain->createSwapChain(window);
    swapchain->createImageViews();
    
    // Recreate render targets
    createRenderTargets();
}

// Begin frame: handle synchronization, image acquisition, and render pass setup
uint32_t Renderer::beginFrame() {
    VkDevice device = dvk->device;

    // Wait for the current frame's fence
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = UINT32_MAX;
    VkResult acquire = vkAcquireNextImageKHR(
        device, *swapchain, UINT64_MAX,
        imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) { framebufferResized = true; return UINT32_MAX; }
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("failed to acquire swapchain image");

    // CRITICAL: If this image is still being used by another frame, wait for that frame's completion
    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(dvk->device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    
    // Reset the current frame's fence ONLY after we're sure the image is free
    vkResetFences(dvk->device, 1, &inFlightFences[currentFrame]);
    
    // Now assign this image to the current frame
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    VkCommandBuffer currentCmd = cmdBuffers[currentFrame];

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(currentCmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    currentRenderTarget = &swapchainRenderTargets[imageIndex];

    // Begin render pass using RenderTarget wrapper
    currentRenderTarget->beginRenderPass(currentCmd);
    currentRenderTarget->setViewportAndScissor(currentCmd);

    return imageIndex;  
}

// Sky rendering using dedicated sky pipeline
void Renderer::drawSky(const tinyProject* project, const PipelineRaster* skyPipeline) const {
    VkCommandBuffer currentCmd = cmdBuffers[currentFrame];

    // Bind sky pipeline
    skyPipeline->bindCmd(currentCmd);

    // Bind only the global descriptor set (set 0) for sky
    const tinyGlobal* global = project->global();
    VkDescriptorSet glbSet = global->getDescSet();
    uint32_t offset = currentFrame * global->alignedSize;
    skyPipeline->bindSets(currentCmd, 0, &glbSet, 1, &offset, 1);

    // Draw fullscreen triangle (3 vertices, no input)
    vkCmdDraw(currentCmd, 3, 1, 0, 0);
}

void Renderer::drawTest(const tinyProject* project, const rtScene* scene, const PipelineRaster* testPipeline) const {
    const SceneRes& sharedRes = scene->res();

    VkCommandBuffer currentCmd = cmdBuffers[currentFrame];

    const tinyGlobal* global = project->global();
    VkDescriptorSet glbSet = global->getDescSet();
    uint32_t offset = currentFrame * global->alignedSize;

    // Bind pipeline and descriptor sets once

    // Use the new instanced static machine
    const tinyDrawable& draw = *sharedRes.drawable;
    const std::vector<ShaderGroup>& shaderGroups = draw.shaderGroups();

    for (const auto& shaderGroup : shaderGroups) {
        const auto& ranges = shaderGroup.instaRanges;
        if (ranges.empty()) continue;

        // Bind this specific shader pipeline
        /* For the time being we only have one pipeline, so no need to switch. */
        testPipeline->bindCmd(currentCmd);
        testPipeline->bindSets(currentCmd, 0, &glbSet, 1, &offset, 1);

        // Bind the instance buffer once then use the ranges to draw
        VkBuffer buffers[] = { draw.instaBuffer() };
        VkDeviceSize offsets[] = { draw.instaSize_x1().aligned * currentFrame };
        vkCmdBindVertexBuffers(currentCmd, 1, 1, buffers, offsets);

        for (const auto& range : ranges) {
            const auto* rMesh = sharedRes.fsGet<tinyMesh>(range.mesh);
            if (!rMesh) continue; // Mesh not found in registry

            // Bind the vertex and index buffers
            VkBuffer vrtxBuffer = rMesh->vrtxBuffer();
            VkBuffer indxBuffer = rMesh->indxBuffer();
            VkIndexType indxType = rMesh->indxType();
            VkBuffer vBuffers[] = { vrtxBuffer };
            VkDeviceSize vOffsets[] = { 0 };
            vkCmdBindVertexBuffers(currentCmd, 0, 1, vBuffers, vOffsets);
            vkCmdBindIndexBuffer(currentCmd, indxBuffer, 0, indxType);

            // Draw each individual submeshes
            for (size_t i = 0; i < rMesh->parts().size(); ++i) {
                uint32_t indxCount = rMesh->parts()[i].indxCount;
                if (indxCount == 0) continue;

                uint32_t indxOffset = rMesh->parts()[i].indxOffset;
                vkCmdDrawIndexed(currentCmd, indxCount, range.count, indxOffset, 0, range.offset);
            }
        }
    }




    // for (const auto& range : ranges) {
    //     const auto* rMesh = sharedRes.fsGet<tinyMesh>(range.mesh);
    //     if (!rMesh) continue; // Mesh not found in registry

    //     // Bind the vertex and index buffers
    //     VkBuffer vrtxBuffer = rMesh->vrtxBuffer();
    //     VkBuffer indxBuffer = rMesh->indxBuffer();
    //     VkIndexType indxType = rMesh->indxType();
    //     VkBuffer vBuffers[] = { vrtxBuffer };
    //     VkDeviceSize vOffsets[] = { 0 };
    //     vkCmdBindVertexBuffers(currentCmd, 0, 1, vBuffers, vOffsets);
    //     vkCmdBindIndexBuffer(currentCmd, indxBuffer, 0, indxType);

    //     // Draw each individual submeshes
    //     for (size_t i = 0; i < rMesh->parts().size(); ++i) {
    //         uint32_t indxCount = rMesh->parts()[i].indxCount;
    //         if (indxCount == 0) continue;

    //         uint32_t indxOffset = rMesh->parts()[i].indxOffset;
    //         vkCmdDrawIndexed(currentCmd, indxCount, range.count, indxOffset, 0, range.offset);
    //     }
    // }
}

// End frame: finalize command buffer, submit, and present
void Renderer::endFrame(uint32_t imageIndex) {
    if (imageIndex == UINT32_MAX) return;

    VkCommandBuffer currentCmd = cmdBuffers[currentFrame];

    // End current render pass
    if (currentRenderTarget) {
        currentRenderTarget->endRenderPass(currentCmd);
        currentRenderTarget = nullptr;
    } else {
        vkCmdEndRenderPass(currentCmd);
    }

    if (vkEndCommandBuffer(currentCmd) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }

    VkSemaphore waitSemaphores[]   = { imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    // SIGNAL the per-IMAGE semaphore:
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[imageIndex] };

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = waitSemaphores;
    submit.pWaitDstStageMask    = &waitStage;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &currentCmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = signalSemaphores;

    if (vkQueueSubmit(dvk->graphicsQueue, 1, &submit, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer");
    }

    // PRESENT waits on the same per-IMAGE semaphore:
    VkPresentInfoKHR present{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = signalSemaphores;
    present.swapchainCount     = 1;
    VkSwapchainKHR chains[]    = { swapchain->get() };
    present.pSwapchains        = chains;
    present.pImageIndices      = &imageIndex;

    VkResult res = vkQueuePresentKHR(dvk->presentQueue, &present);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = true;
    } else if (res != VK_SUCCESS) {
        throw std::runtime_error("failed to present");
    }

    currentFrame = (currentFrame + 1) % maxFramesInFlight;
}

void Renderer::processPendingRemovals(tinyProject* project, rtScene* activeScene) {
    if (!project->hasDeferredRms(tinyProject::DeferRmType::Vulkan)) return;

    // Wait for ALL in-flight fences to ensure no resources are in use by GPU
    // This is the safest approach - wait for all frames to complete
    std::vector<VkFence> allFences;
    for (size_t i = 0; i < maxFramesInFlight; ++i) {
        if (inFlightFences[i] != VK_NULL_HANDLE) {
            allFences.push_back(inFlightFences[i]);
        }
    }
    
    if (!allFences.empty()) {
        // Wait for all frames to complete with a reasonable timeout (1 second)
        VkResult result = vkWaitForFences(dvk->device, 
                                        static_cast<uint32_t>(allFences.size()), 
                                        allFences.data(), 
                                        VK_TRUE, 
                                        1000000000); // 1 second timeout in nanoseconds

        if (result == VK_TIMEOUT) {
            // Log warning but continue - GPU might be hung, but we can't wait forever
            printf("Warning: Timeout waiting for GPU to finish before deleting resources\n");
        } else if (result != VK_SUCCESS) {
            return; // Don't delete if we can't confirm GPU is done
        }
    }

    project->execDeferredRms(tinyProject::DeferRmType::Vulkan);

    // FOR THE TIME BEING IGNORE THE ABOVE AND DO NOTHING
}
